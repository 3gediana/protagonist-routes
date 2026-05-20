#include "core/observer/ComprehensiveMetrics.h"

#include "core/agent/Agent.h"
#include "core/events/EventBus.h"
#include "core/events/EventPayloads.h"
#include "core/interfaces/IWorld.h"
#include "core/logging/Logger.h"
#include "core/world/layers/AgentLayer.h"
#include "core/world/layers/signal/SignalLayer.h"
#include "core/world/layers/terrain/TerrainLayer.h"
#include "core/world/layers/TimeOfDayLayer.h"
#include "core/world/layers/object/ObjectLayer.h"
#include "core/brain/neat/NeatBrain.h"
#include "core/pathogen/Pathogen.h"
#include "core/capability/PredatorHuntCapability.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>

namespace neuro {

namespace {

constexpr std::uint16_t kActionMove = 1u << 0;
constexpr std::uint16_t kActionEat = 1u << 1;
constexpr std::uint16_t kActionAttack = 1u << 2;
constexpr std::uint16_t kActionSignal = 1u << 3;
constexpr std::uint16_t kActionFlee = 1u << 4;
constexpr std::uint16_t kActionCarry = 1u << 5;
constexpr std::uint16_t kActionTerrain = 1u << 6;
constexpr std::uint16_t kActionMate = 1u << 7;

}

ComprehensiveMetrics::ComprehensiveMetrics(const std::filesystem::path& output_dir)
    : output_dir_(output_dir),
      spatial_entropy_accum_(kSpatialBins * kSpatialBins, 0.0),
      action_bucket_accum_(kActionBuckets, 0.0),
      signal_action_joint_accum_(3 * kActionBuckets, 0.0),
      terrain_action_joint_accum_(5 * kActionBuckets, 0.0),
      infection_action_joint_accum_(3 * kActionBuckets, 0.0) {
    std::filesystem::create_directories(output_dir);

    oee_csv_.open(output_dir_ / "oee_complexity.csv", std::ios::out | std::ios::trunc);
    oee_csv_ << "gen,avg_nodes,avg_connections,avg_hidden,max_nodes,max_connections,structural_diversity\n";
    pop_csv_.open(output_dir_ / "population_stats.csv", std::ios::out | std::ios::trunc);
    pop_csv_ << "gen,shannon_diversity,species_count,avg_species_size,max_species_size,"
             << "species_size_entropy,spatial_entropy,behavioral_entropy,fitness_entropy,"
             << "avg_energy,energy_variance,alive_ratio,avg_age,pred_count,prey_count,"
             << "infected_ratio,immune_ratio,carrying_ratio\n";
    info_csv_.open(output_dir_ / "info_theory.csv", std::ios::out | std::ios::trunc);
    info_csv_ << "gen,signal_field_entropy,action_entropy,"
              << "signal_action_mi,terrain_behavior_mi,infection_behavior_mi\n";
    coevo_csv_.open(output_dir_ / "coevo_stats.csv", std::ios::out | std::ios::trunc);
    coevo_csv_ << "gen,pred_best_fitness,prey_best_fitness,pred_avg_fitness,prey_avg_fitness,"
               << "red_queen_corr,hunt_success_rate,pred_prey_size_ratio\n";
    disease_csv_.open(output_dir_ / "disease_arms_race.csv", std::ios::out | std::ios::trunc);
    disease_csv_ << "gen,avg_virulence,max_virulence,strain_count,strain_diversity,"
                 << "avg_immunity_rate,immune_memory_size,infection_rate\n";
    species_profile_csv_.open(output_dir_ / "species_profile.csv", std::ios::out | std::ios::trunc);
    species_profile_csv_ << "gen,species_id,population,avg_x,avg_y,avg_energy,"
                         << "pct_predator,pct_infected,pct_in_cave,pct_carrying,"
                         << "avg_nodes,avg_connections,avg_fitness,"
                         << "avg_habitat_pref,avg_activity_period,avg_diet_pref\n";
}

ComprehensiveMetrics::~ComprehensiveMetrics() {
    if (oee_csv_.is_open()) oee_csv_.close();
    if (pop_csv_.is_open()) pop_csv_.close();
    if (info_csv_.is_open()) info_csv_.close();
    if (coevo_csv_.is_open()) coevo_csv_.close();
    if (disease_csv_.is_open()) disease_csv_.close();
    if (species_profile_csv_.is_open()) species_profile_csv_.close();
}

void ComprehensiveMetrics::onAttach(IWorld& world) {
    world_ = &world;
    auto& bus = world.eventBus();
    gen_sub_id_ = bus.subscribe(events::EventType::GenerationEnded,
        [this](const events::Event& event) {
            const auto& payload = event.payloadAs<events::GenerationEnded>();
            onGenerationEnded(payload.gen);
        });
    tick_sub_id_ = bus.subscribe(events::EventType::TickEnded,
        [this](const events::Event&) { onTickEnded(); });
    any_sub_id_ = bus.subscribeAny(
        [this](const events::Event& event) {
            switch (event.type()) {
                case events::EventType::AgentMoved: {
                    const auto& payload = event.payloadAs<events::AgentMoved>();
                    tick_action_flags_[payload.id] |= kActionMove;
                    break;
                }
                case events::EventType::AgentAteFood: {
                    const auto& payload = event.payloadAs<events::AgentAteFood>();
                    tick_action_flags_[payload.id] |= kActionEat;
                    break;
                }
                case events::EventType::AgentAttacked: {
                    const auto& payload = event.payloadAs<events::AgentAttacked>();
                    tick_action_flags_[payload.attacker] |= kActionAttack;
                    break;
                }
                case events::EventType::AgentEmittedSignal: {
                    const auto& payload = event.payloadAs<events::AgentEmittedSignal>();
                    tick_action_flags_[payload.id] |= kActionSignal;
                    break;
                }
                case events::EventType::AgentFled: {
                    const auto& payload = event.payloadAs<events::AgentFled>();
                    tick_action_flags_[payload.id] |= static_cast<std::uint16_t>(kActionMove | kActionFlee);
                    break;
                }
                case events::EventType::ObjectPickedUp: {
                    const auto& payload = event.payloadAs<events::ObjectPickedUp>();
                    tick_action_flags_[payload.id] |= kActionCarry;
                    break;
                }
                case events::EventType::ObjectDropped: {
                    const auto& payload = event.payloadAs<events::ObjectDropped>();
                    tick_action_flags_[payload.id] |= kActionCarry;
                    break;
                }
                case events::EventType::TerrainModified: {
                    const auto& payload = event.payloadAs<events::TerrainModified>();
                    tick_action_flags_[payload.id] |= kActionTerrain;
                    break;
                }
                case events::EventType::MatingOccurred: {
                    const auto& payload = event.payloadAs<events::MatingOccurred>();
                    tick_action_flags_[payload.initiator] |= kActionMate;
                    tick_action_flags_[payload.partner] |= kActionMate;
                    break;
                }
                default:
                    break;
            }
        });
}

void ComprehensiveMetrics::onDetach(IWorld& world) {
    auto& bus = world.eventBus();
    bus.unsubscribe(gen_sub_id_);
    bus.unsubscribe(tick_sub_id_);
    bus.unsubscribe(any_sub_id_);
    world_ = nullptr;
}

void ComprehensiveMetrics::onTickEnded() {
    tick_count_++;
    const auto* sig = world_ ? world_->getLayer<SignalLayer>() : nullptr;
    if (sig) signal_energy_history_.push_back(sig->totalEnergy());
    const auto* al = world_ ? world_->getLayer<AgentLayer>() : nullptr;
    const auto* terrain = world_ ? world_->getLayer<TerrainLayer>() : nullptr;
    if (!world_ || !al) {
        tick_action_flags_.clear();
        return;
    }

    const Vec2 world_size = world_->size();
    const float world_x = std::max(world_size.x, 1.0f);
    const float world_y = std::max(world_size.y, 1.0f);
    const float signal_sample_radius = 0.075f * std::max(1.0f, std::min(world_x, world_y));
    if (al) {
        for (const Agent* a : al->agents()) {
            if (!a->alive()) continue;
            const auto& pos = a->position();
            std::size_t bx = std::min(static_cast<std::size_t>(pos.x / world_x * kSpatialBins), kSpatialBins - 1);
            std::size_t by = std::min(static_cast<std::size_t>(pos.y / world_y * kSpatialBins), kSpatialBins - 1);
            spatial_entropy_accum_[bx * kSpatialBins + by] += 1.0;

            const auto action_it = tick_action_flags_.find(a->id());
            const std::size_t bucket = action_it == tick_action_flags_.end()
                ? 0
                : std::min<std::size_t>(action_it->second, kActionBuckets - 1);
            action_bucket_accum_[bucket] += 1.0;

            if (sig) {
                auto sample = sig->sample(pos, signal_sample_radius);
                float signal_strength = 0.0f;
                for (float value : sample) {
                    signal_strength += std::abs(value);
                }
                const int signal_bin = signal_strength > 0.5f ? 2 : (signal_strength > 0.05f ? 1 : 0);
                signal_action_joint_accum_[signal_bin * kActionBuckets + bucket] += 1.0;
            }

            if (terrain) {
                const float height_scale = std::max(terrain->heightScale(), 1.0f);
                const float normalized_height = std::clamp(terrain->heightAt(pos.x, pos.y) / height_scale, 0.0f, 0.9999f);
                const int terrain_bin = std::min(static_cast<int>(normalized_height * 5.0f), 4);
                terrain_action_joint_accum_[terrain_bin * kActionBuckets + bucket] += 1.0;
            }

            const int infection_bin = a->infected() ? 2 : (a->immune() ? 1 : 0);
            infection_action_joint_accum_[infection_bin * kActionBuckets + bucket] += 1.0;

            disease_immunity_rate_sum_ += a->immunityGainRate();
            disease_immune_memory_sum_ += static_cast<double>(a->immuneMemory().size());
            ++disease_alive_agent_samples_;
            if (a->infected()) {
                ++disease_infected_agent_samples_;
                const Pathogen* pathogen = a->pathogen();
                if (pathogen != nullptr) {
                    const double virulence = pathogen->virulence();
                    disease_virulence_sum_ += virulence;
                    if (virulence > disease_max_virulence_) {
                        disease_max_virulence_ = virulence;
                    }
                    disease_strain_counts_[pathogen->strainSignature()]++;
                }
            }
        }
    }
    tick_action_flags_.clear();
}

void ComprehensiveMetrics::onGenerationEnded(Generation generation) {
    gen_ = generation;
    const auto* al = world_ ? world_->getLayer<AgentLayer>() : nullptr;
    if (!al) return;
    auto agents = al->agents();
    const std::size_t n_total = agents.size();
    if (n_total == 0) return;

    const std::vector<const Agent*> all_agents(agents.begin(), agents.end());
    std::vector<const Agent*> alive_agents;
    alive_agents.reserve(all_agents.size());
    for (const Agent* agent : all_agents) {
        if (agent->alive()) {
            alive_agents.push_back(agent);
        }
    }
    const auto& sampled_agents = alive_agents.empty() ? all_agents : alive_agents;
    const std::size_t n = sampled_agents.size();
    const std::size_t n_alive = alive_agents.size();

    std::vector<double> action_probs(kActionBuckets, 0.0);
    double total_actions = std::accumulate(action_bucket_accum_.begin(), action_bucket_accum_.end(), 0.0);
    if (total_actions > 0.0) {
        for (std::size_t i = 0; i < action_bucket_accum_.size(); ++i) {
            action_probs[i] = action_bucket_accum_[i] / total_actions;
        }
    }
    const double action_entropy = computeEntropy(action_probs);

    // OEE
    {
        double sn = 0, sc = 0, sh = 0, mn = 0, mc = 0;
        std::vector<double> ncs;
        for (const Agent* a : sampled_agents) {
            const auto* nb = dynamic_cast<const NeatBrain*>(&a->brain());
            if (nb) {
                double nn = static_cast<double>(nb->nodeCount());
                double nc = static_cast<double>(nb->connectionCount());
                sn += nn; sc += nc; sh += static_cast<double>(nb->hiddenCount());
                if (nn > mn) mn = nn; if (nc > mc) mc = nc;
                ncs.push_back(nn);
            }
        }
        double sd = 0;
        if (ncs.size() > 1) { double m2 = sn/n; double v=0; for(auto c:ncs) v+=(c-m2)*(c-m2); v/=n; sd = m2>0?std::sqrt(v)/m2:0; }
        oee_csv_ << gen_ << "," << (sn/n) << "," << (sc/n) << "," << (sh/n) << "," << mn << "," << mc << "," << sd << "\n";
    }

    // Population
    {
        std::map<SpeciesId, std::size_t> sp_cnt;
        double se=0,sa=0; std::size_t pred=0,inf=0,imm=0,car=0;
        std::vector<double> ens, fits;
        for (const Agent* a : sampled_agents) {
            sp_cnt[a->speciesId()]++; se+=a->energy(); sa+=a->ageSeconds();
            ens.push_back(a->energy()); fits.push_back(a->fitness());
            if(a->isPredator())++pred; if(a->infected())++inf; if(a->immune())++imm; if(a->isCarrying())++car;
        }
        double shannon=0; for(auto&[s,c]:sp_cnt){double p=double(c)/n;if(p>0)shannon-=p*std::log2(p);}
        std::size_t mx=0; for(auto&[s,c]:sp_cnt)if(c>mx)mx=c;
        double stot=0;for(auto v:spatial_entropy_accum_)stot+=v;double sent=0;if(stot>0)for(auto v:spatial_entropy_accum_){double p=v/stot;if(p>0)sent-=p*std::log2(p);}
        double fe=0;if(fits.size()>=2){double fmn=*std::min_element(fits.begin(),fits.end()),fmx=*std::max_element(fits.begin(),fits.end()),fr=fmx-fmn;if(fr>1e-6){std::vector<double>fb(10,0);for(double f:fits)fb[std::min(static_cast<int>((f-fmn)/fr*9),9)]+=1;for(auto b:fb){double p=b/n;if(p>0)fe-=p*std::log2(p);}}}
        double ae2=se/n,ev=0;for(double e:ens)ev+=(e-ae2)*(e-ae2);ev/=n;
        pop_csv_<<gen_<<","<<shannon<<","<<sp_cnt.size()<<","<<(n/std::max(double(sp_cnt.size()),1.0))<<","<<mx<<","<<shannon<<","<<sent<<","<<action_entropy<<","<<fe<<","<<ae2<<","<<ev<<","<<(double(n_alive)/n_total)<<","<<(sa/n)<<","<<pred<<","<<(n-pred)<<","<<(double(inf)/n)<<","<<(double(imm)/n)<<","<<(double(car)/n)<<"\n";
    }

    // Info theory
    {
        double sfe=0;if(!signal_energy_history_.empty()){double te=0;for(auto e:signal_energy_history_)te+=e;if(te>0)for(auto e:signal_energy_history_){double p=e/te;if(p>0)sfe-=p*std::log2(p);}}
        const double sami = computeMIFromCounts(signal_action_joint_accum_, 3, static_cast<int>(kActionBuckets));
        const double tmi = computeMIFromCounts(terrain_action_joint_accum_, 5, static_cast<int>(kActionBuckets));
        const double imi = computeMIFromCounts(infection_action_joint_accum_, 3, static_cast<int>(kActionBuckets));
        info_csv_<<gen_<<","<<sfe<<","<<action_entropy<<","<<sami<<","<<tmi<<","<<imi<<"\n";
    }

    // Coevo
    {
        double pb=0,pyb=0,ps=0,pys=0;std::size_t pn=0,pyn=0;
        for(const Agent*a:sampled_agents){if(a->isPredator()){ps+=a->fitness();if(a->fitness()>pb)pb=a->fitness();++pn;}else{pys+=a->fitness();if(a->fitness()>pyb)pyb=a->fitness();++pyn;}}
        double hr=0;{auto at=PredatorHuntCapability::hunt_attempts.load(std::memory_order_relaxed);auto su=PredatorHuntCapability::hunt_successes.load(std::memory_order_relaxed);if(at>0)hr=double(su)/double(at);PredatorHuntCapability::resetCounters();}
        pred_best_history_.push_back(pb);prey_best_history_.push_back(pyb);if(pred_best_history_.size()>kRedQueenWindow){pred_best_history_.erase(pred_best_history_.begin());prey_best_history_.erase(prey_best_history_.begin());}
        double rq=0;if(pred_best_history_.size()>=10)rq=computePearson(pred_best_history_,prey_best_history_);
        coevo_csv_<<gen_<<","<<pb<<","<<pyb<<","<<(pn?ps/pn:0)<<","<<(pyn?pys/pyn:0)<<","<<rq<<","<<hr<<","<<(pyn?double(pn)/double(pyn):0)<<"\n";
    }

    // Disease
    {
        if (disease_alive_agent_samples_ > 0) {
            double strain_diversity = 0.0;
            if (!disease_strain_counts_.empty()) {
                double total = 0.0;
                for (const auto& [strain, count] : disease_strain_counts_) {
                    (void)strain;
                    total += static_cast<double>(count);
                }
                if (total > 0.0) {
                    for (const auto& [strain, count] : disease_strain_counts_) {
                        (void)strain;
                        const double p = static_cast<double>(count) / total;
                        if (p > 0.0) {
                            strain_diversity -= p * std::log2(p);
                        }
                    }
                }
            }

            disease_csv_ << gen_ << ","
                          << (disease_infected_agent_samples_ ? disease_virulence_sum_ / disease_infected_agent_samples_ : 0.0) << ","
                          << disease_max_virulence_ << ","
                          << disease_strain_counts_.size() << ","
                          << strain_diversity << ","
                          << (disease_immunity_rate_sum_ / disease_alive_agent_samples_) << ","
                          << (disease_immune_memory_sum_ / disease_alive_agent_samples_) << ","
                          << (static_cast<double>(disease_infected_agent_samples_) / disease_alive_agent_samples_) << "\n";
        } else {
            double vs=0,vm=0;std::size_t pc=0;std::map<PathogenGenome::StrainSignature,std::size_t>sm;double igs=0;std::size_t imt=0,inf_n=0;
            for(const Agent*a:sampled_agents){if(a->infected()){++inf_n;const Pathogen*p=a->pathogen();if(p){double v=p->virulence();vs+=v;if(v>vm)vm=v;++pc;sm[p->strainSignature()]++;}}igs+=a->immunityGainRate();imt+=a->immuneMemory().size();}
            double sd=0;if(!sm.empty()){double t=0;for(auto&[s,c]:sm)t+=c;for(auto&[s,c]:sm){double p=c/t;if(p>0)sd-=p*std::log2(p);}}
            disease_csv_<<gen_<<","<<(pc?vs/pc:0)<<","<<vm<<","<<sm.size()<<","<<sd<<","<<(igs/n)<<","<<(double(imt)/n)<<","<<(double(inf_n)/n)<<"\n";
        }
    }

    // Species profile
    {
        const auto*ter=world_->getLayer<TerrainLayer>();
        std::map<SpeciesId,std::vector<const Agent*>>sm;for(const Agent*a:sampled_agents)sm[a->speciesId()].push_back(a);
        for(auto&[sid,mem]:sm){double ax=0,ay=0,ae=0,af=0;std::size_t pn=0,in=0,cn=0,crn=0;double sn=0,sc=0;
        double hab_sum=0,act_sum=0,diet_sum=0;
        for(const Agent*a:mem){ax+=a->position().x;ay+=a->position().y;ae+=a->energy();af+=a->fitness();if(a->isPredator())++pn;if(a->infected())++in;if(a->isCarrying())++crn;if(ter&&ter->inCave(a->position().x,a->position().y))++cn;const auto*nb=dynamic_cast<const NeatBrain*>(&a->brain());if(nb){sn+=nb->nodeCount();sc+=nb->connectionCount();}
        hab_sum+=a->habitatPreference();act_sum+=a->activityPeriod();diet_sum+=a->dietPreference();
        }
        std::size_t m=mem.size();species_profile_csv_<<gen_<<","<<static_cast<std::uint32_t>(sid.value)<<","<<m<<","<<(ax/m)<<","<<(ay/m)<<","<<(ae/m)<<","<<(double(pn)/m)<<","<<(double(in)/m)<<","<<(double(cn)/m)<<","<<(double(crn)/m)<<","<<(sn/m)<<","<<(sc/m)<<","<<(af/m)<<","<<(hab_sum/m)<<","<<(act_sum/m)<<","<<(diet_sum/m)<<"\n";}
    }

    signal_energy_history_.clear();
    spatial_entropy_accum_.assign(kSpatialBins * kSpatialBins, 0.0);
    action_bucket_accum_.assign(kActionBuckets, 0.0);
    signal_action_joint_accum_.assign(3 * kActionBuckets, 0.0);
    terrain_action_joint_accum_.assign(5 * kActionBuckets, 0.0);
    infection_action_joint_accum_.assign(3 * kActionBuckets, 0.0);
    disease_virulence_sum_ = 0.0;
    disease_max_virulence_ = 0.0;
    disease_immunity_rate_sum_ = 0.0;
    disease_immune_memory_sum_ = 0.0;
    disease_alive_agent_samples_ = 0;
    disease_infected_agent_samples_ = 0;
    disease_strain_counts_.clear();
    tick_action_flags_.clear();
    tick_count_ = 0;
    oee_csv_.flush(); pop_csv_.flush(); info_csv_.flush(); coevo_csv_.flush(); disease_csv_.flush(); species_profile_csv_.flush();
}

double ComprehensiveMetrics::computeEntropy(const std::vector<double>& probs) {
    double h = 0; for (double p : probs) if (p > 0) h -= p * std::log2(p); return h;
}

double ComprehensiveMetrics::computePearson(const std::vector<double>& x, const std::vector<double>& y) {
    const std::size_t n = std::min(x.size(), y.size());
    if (n < 3) return 0;
    double sx=0,sy=0,sxy=0,sx2=0,sy2=0;
    for (std::size_t i = 0; i < n; ++i) { sx+=x[i];sy+=y[i];sxy+=x[i]*y[i];sx2+=x[i]*x[i];sy2+=y[i]*y[i]; }
    double d = std::sqrt((n*sx2-sx*sx)*(n*sy2-sy*sy));
    return d > 1e-12 ? (n*sxy-sx*sy)/d : 0;
}

double ComprehensiveMetrics::computeMI(const std::vector<std::pair<int,int>>& joint, int kx, int ky) {
    if (joint.empty()) return 0;
    std::vector<double> px(kx,0),py(ky,0);std::map<std::pair<int,int>,double>pxy;
    const double n=static_cast<double>(joint.size());
    for (const auto& [x,y] : joint) { if(x>=0&&x<kx)px[x]+=1.0/n;if(y>=0&&y<ky)py[y]+=1.0/n;pxy[{x,y}]+=1.0/n; }
    double mi=0;for(const auto&[xy,p]:pxy){if(p>0&&xy.first<kx&&xy.second<ky&&px[xy.first]>0&&py[xy.second]>0)mi+=p*std::log2(p/(px[xy.first]*py[xy.second]));}
    return mi;
}

double ComprehensiveMetrics::computeMIFromCounts(const std::vector<double>& joint_counts, int kx, int ky) {
    if (joint_counts.empty()) return 0;
    const double total = std::accumulate(joint_counts.begin(), joint_counts.end(), 0.0);
    if (total <= 0.0) return 0;

    std::vector<double> px(kx, 0.0), py(ky, 0.0);
    for (int x = 0; x < kx; ++x) {
        for (int y = 0; y < ky; ++y) {
            const double p = joint_counts[static_cast<std::size_t>(x) * ky + y] / total;
            px[x] += p;
            py[y] += p;
        }
    }

    double mi = 0.0;
    for (int x = 0; x < kx; ++x) {
        for (int y = 0; y < ky; ++y) {
            const double p = joint_counts[static_cast<std::size_t>(x) * ky + y] / total;
            if (p > 0.0 && px[x] > 0.0 && py[y] > 0.0) {
                mi += p * std::log2(p / (px[x] * py[y]));
            }
        }
    }
    return mi;
}

}  // namespace neuro
