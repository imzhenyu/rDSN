/*
* Description:
*     helper class to manage a remote safe pointer
*
* Revision history:
*     Mar., 2017, @imzhenyu (Zhenyu Guo), first version
*     xxxx-xx-xx, author, fix bug about xxx
*/

# pragma once

# include <string>
# include <vector>
# include <queue>

namespace dsn
{
    class safe_pointer_manager
    {
    public:
        safe_pointer_manager(int count, const char* name)
        {
            _pointers.reserve(count);
            for (int i = 0; i < count; i++)
            {
                _pointers.emplace_back(std::make_pair(1, nullptr));
                _free_slots.emplace(i);
            }
        }

        // index = idx << 32 | version
        uint64_t save(void* obj)
        {
            dassert(_free_slots.size() > 0,
                "no more slots for safe pointer manager %s - you may need to adjust your configuration",
                _name.c_str()
            );

            int slot = _free_slots.front();
            _free_slots.pop();

            auto& pr = _pointers[slot];
            dassert(pr.second == nullptr, "this slot is already occupied in %s.%d", _name.c_str(), slot);
            pr.second = obj;
            return (((uint64_t)slot) << 32) | (pr.first);
        }

        bool destroy(uint64_t index, void* obj)
        {
            int slot = (int)(index >> 32);
            int version = (int)(index & 0xffffffffULL);

            auto& pr = _pointers[slot];
            if (pr.first == version &&pr.second == obj)
            {
                ++pr.first;
                pr.second = nullptr;
                _free_slots.push(slot);
                return true;
            }
            else
                return false;
        }

        void* get(uint64_t index)
        {
            int slot = (int)(index >> 32);
            int version = (int)(index & 0xffffffffULL);

            auto& pr = _pointers[slot];
            if (pr.first == version)
                return pr.second;
            else
                return nullptr;
        }

    private:
        std::vector<std::pair<int, void*>> _pointers; //version, obj
        std::queue<int>  _free_slots;
        std::string      _name;
    };
}
