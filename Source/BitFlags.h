#pragma once

#include <type_traits>

template <typename EnumType>
struct BitFlags
{
  static_assert(std::is_enum<EnumType>::value, "BitFlags requires an enum type.");

private:
  using ValueType = typename std::underlying_type<EnumType>::type;
  using ThisType = BitFlags<EnumType>;

  ValueType m_value = 0;

public:
  // Constructors
  constexpr BitFlags() = default;
  constexpr BitFlags(EnumType flag) : m_value(static_cast<ValueType>(flag)) {}
  constexpr BitFlags(ValueType val) : m_value(val) {}

  // Set flag(s)
  void set(EnumType flag) { m_value |= static_cast<ValueType>(flag); }

  // Unset flag(s)
  void unset(EnumType flag) { m_value &= ~static_cast<ValueType>(flag); }

  // Flip flag(s)
  void flip(EnumType flag) { m_value ^= static_cast<ValueType>(flag); }

  // Clear all
  void clear() { m_value = 0; }

  // Check exact match
  constexpr bool is_set(EnumType flag) const
  {
    return (m_value & static_cast<ValueType>(flag)) == static_cast<ValueType>(flag);
  }

  constexpr bool any() const { return m_value != 0; }
  constexpr bool none() const { return m_value == 0; }

  // Get raw value
  constexpr ValueType value() const { return m_value; }

  // Operator overloads
  ThisType &operator|=(EnumType flag)
  {
    set(flag);
    return *this;
  }

  ThisType &operator&=(EnumType flag)
  {
    m_value &= static_cast<ValueType>(flag);
    return *this;
  }
  
  ThisType &operator^=(EnumType flag)
  {
    flip(flag);
    return *this;
  }

  // Comparisons
  constexpr bool operator==(const ThisType &other) const { return m_value == other.m_value; }
  constexpr bool operator!=(const ThisType &other) const { return m_value != other.m_value; }
};

// Helper macro to define bitmask operators for a specific enum type
#define DEFINE_BITMASK_OPERATORS(EnumType)                                      \
inline constexpr EnumType operator|(EnumType lhs, EnumType rhs)                \
{                                                                              \
    using Underlying = std::underlying_type_t<EnumType>;                      \
    return static_cast<EnumType>(                                              \
        static_cast<Underlying>(lhs) | static_cast<Underlying>(rhs));         \
}                                                                              \
inline constexpr EnumType operator&(EnumType lhs, EnumType rhs)               \
{                                                                              \
    using Underlying = std::underlying_type_t<EnumType>;                      \
    return static_cast<EnumType>(                                              \
        static_cast<Underlying>(lhs) & static_cast<Underlying>(rhs));         \
}                                                                              \
inline constexpr EnumType operator^(EnumType lhs, EnumType rhs)               \
{                                                                              \
    using Underlying = std::underlying_type_t<EnumType>;                      \
    return static_cast<EnumType>(                                              \
        static_cast<Underlying>(lhs) ^ static_cast<Underlying>(rhs));         \
}                                                                              \
inline constexpr EnumType operator~(EnumType value)                            \
{                                                                              \
    using Underlying = std::underlying_type_t<EnumType>;                      \
    return static_cast<EnumType>(~static_cast<Underlying>(value));            \
}                                                                              \
inline EnumType& operator|=(EnumType& lhs, EnumType rhs)                       \
{                                                                              \
    lhs = lhs | rhs;                                                           \
    return lhs;                                                                \
}                                                                              \
inline EnumType& operator&=(EnumType& lhs, EnumType rhs)                       \
{                                                                              \
    lhs = lhs & rhs;                                                           \
    return lhs;                                                                \
}                                                                              \
inline EnumType& operator^=(EnumType& lhs, EnumType rhs)                       \
{                                                                              \
    lhs = lhs ^ rhs;                                                           \
    return lhs;                                                                \
}

