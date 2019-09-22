#ifndef ENUMBITSET_HPP
#define ENUMBITSET_HPP


template<typename EnumT, typename UnderlyingIntT>
class Bitset
{
public:
    Bitset()
        : m_raw(0)
    {}

    bool operator[](EnumT position) const
    {
        return test(position);
    }

    Bitset& operator<<(EnumT position)
    {
        set(position);
        return *this;
    }

    Bitset& unset(EnumT position)
    {
        m_raw = m_raw & ~(1 << (int)position);
        return *this;
    }

    Bitset& set(EnumT position)
    {
        m_raw = m_raw | (1 << (int)position);
        return *this;
    }

    bool test(EnumT position) const
    {
        return (m_raw & (1 << (int)position)) != 0;
    }

private:
    UnderlyingIntT m_raw;
};


#endif // ENUMBITSET_HPP
