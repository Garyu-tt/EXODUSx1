#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <mutex>

namespace exodus::hardware {
// Accounting only: no real allocation, mapping, cache operation or fence wait.
class BufferLedger {
public:
    enum class Stage { allocated, mapped, submitted, complete, unmapped, restored, quarantined };
    enum class Event { map_ok, submit_ok, fence_ok, unmap_ok, cache_ok, release, uncertain };
    enum class Result { ok, invalid, budget, stopped, missing, wrong_stage };
    struct Reservation { Result result; std::uint64_t id=0; };
    struct Snapshot { std::size_t used, quarantined, count; bool stopped; };
private:
    struct Buffer { std::size_t bytes; Stage stage; };
    const std::size_t limit_, quarantine_limit_, slots_;
    mutable std::mutex lock_;
    std::map<std::uint64_t,Buffer> buffers_;
    std::size_t used_=0, quarantined_=0;
    std::uint64_t next_=1;
    bool stopped_=false;
public:
    BufferLedger(std::size_t limit,std::size_t quarantine_limit,std::size_t slots=64)
        :limit_(limit),quarantine_limit_(quarantine_limit),slots_(slots){}
    Reservation reserve(std::size_t bytes,bool owned){
        std::lock_guard<std::mutex> guard(lock_);
        if(!owned||!bytes||!limit_||!slots_||quarantine_limit_>limit_)return {Result::invalid};
        if(stopped_)return {Result::stopped};
        if(bytes>limit_-used_||buffers_.size()>=slots_||next_==std::numeric_limits<std::uint64_t>::max())return {Result::budget};
        const auto id=next_;
        buffers_.emplace(id,Buffer{bytes,Stage::allocated});
        ++next_;used_+=bytes;
        return {Result::ok,id};
    }
    Result apply(std::uint64_t id,Event event){
        std::lock_guard<std::mutex> guard(lock_);
        auto it=buffers_.find(id);if(it==buffers_.end())return Result::missing;
        auto& b=it->second;
        if(b.stage==Stage::quarantined)return Result::wrong_stage;
        if(event==Event::uncertain){
            b.stage=Stage::quarantined;quarantined_+=b.bytes;
            if(quarantined_>=quarantine_limit_)stopped_=true;
            return Result::ok;
        }
        if(event==Event::release){
            if(b.stage!=Stage::allocated&&b.stage!=Stage::restored)return Result::wrong_stage;
            used_-=b.bytes;buffers_.erase(it);return Result::ok;
        }
        switch(event){
            case Event::map_ok: if(b.stage==Stage::allocated){b.stage=Stage::mapped;return Result::ok;}break;
            case Event::submit_ok: if(b.stage==Stage::mapped&&!stopped_){b.stage=Stage::submitted;return Result::ok;}break;
            case Event::fence_ok: if(b.stage==Stage::submitted){b.stage=Stage::complete;return Result::ok;}break;
            case Event::unmap_ok: if(b.stage==Stage::complete||b.stage==Stage::mapped){b.stage=Stage::unmapped;return Result::ok;}break;
            case Event::cache_ok: if(b.stage==Stage::unmapped){b.stage=Stage::restored;return Result::ok;}break;
            default:break;
        }
        return Result::wrong_stage;
    }
    Snapshot snapshot() const {
        std::lock_guard<std::mutex> guard(lock_);
        return {used_,quarantined_,buffers_.size(),stopped_};
    }
};
} // namespace exodus::hardware
