/*
 * Hash table for different purposes.
 */

#ifndef CTF_HASH_H_INCLUDED
#define CTF_HASH_H_INCLUDED

#include <map>

#include <cctype>

/*
 * Simple implementation - via std::map.
 *
 * class Key should have method
 *
 * THash hash() const.
 *
 * Both classed Key and THash should support == operation.
 * Class THash should be integer-like type(e.g. support % operator).
 *
 * Also, because of implementation, class Key should support operator<.
 */


template<class Key, class Value, class THash = int>
class HashTable
{
public:
    typedef typename std::pair<THash, Key> key_type_real;
    typedef typename std::map<key_type_real, Value>::iterator iterator;
    typedef typename std::map<key_type_real, Value>::const_iterator const_iterator;
    typedef typename std::pair<const key_type_real, Value> value_type;

    HashTable(void) : mapping() {};
    HashTable(const HashTable<Key, Value, THash>& hashTable)
        : mapping(hashTable.mapping) {}
    
    std::pair<iterator, bool> insert (Key key, Value value)
    {
        value_type v(key_type_real(key.hash(), key), value);
        return mapping.insert(v);
    }
    const_iterator find(Key key) const
    {
        return mapping.find(key_type_real(key.hash(), key));
    }
    
    void clear(void)
    {
        mapping.clear();
    }
    
    void swap(HashTable<Key, Value, THash>& hashTable)
    {
        mapping.swap(hashTable.mapping);
    }
    
    iterator begin(void) {return mapping.begin();}
    const_iterator begin(void) const {return mapping.begin();}
    
    iterator end(void) {return mapping.end();}
    const_iterator end(void) const {return mapping.end();}

private:
    std::map<key_type_real, Value> mapping;
};

/*
 * ID comparision helpers.
 * 
 * ID is (sub)string, contained only letters, digits or underscore.
 */

class IDHelpers
{
public:
    static bool isID(char c) {return idTable[(unsigned char)c];}
    static bool less(const char* id1, const char* id2)
    {
        for(;isID(*id1); ++id1, ++id2)
        {
            if(*id1 < *id2 ) return true;
            else if(*id1 > *id2 ) return false;
        }
        return isID(*id2);
    }
    
    static unsigned int hash(const char* id)
    {
        unsigned value = 0;
        for(; isID(*id); id++)
        {
            value = value * 101 + *id;
        }
        return value;
    }

    IDHelpers(void);
private:
    static char idTable[256];
};

#endif // CTF_HASH_H_INCLUDED
