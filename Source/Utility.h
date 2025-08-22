#pragma once

#include <Log.h>
#include <CoreDefs.h>
#include <MathLib.h>
#include <Platform/PlatformContext.h>

//------------------------------------------------------------------------------------------------------------------------------------
// NonCopyable and NonMovable
//------------------------------------------------------------------------------------------------------------------------------------

class NonCopyable
{
protected:
  NonCopyable() = default;

  NonCopyable(const NonCopyable &) = delete;
  NonCopyable &operator=(const NonCopyable &) = delete;
};

class NonMovable : public NonCopyable
{
protected:
  NonMovable() = default;

  NonMovable(NonMovable &&) = delete;
  NonMovable &operator=(NonMovable &&) = delete;
};

//------------------------------------------------------------------------------------------------------------------------------------
// Singleton
//------------------------------------------------------------------------------------------------------------------------------------

template <typename Class>
class Singleton : private NonMovable
{
public:
  using BaseType = Singleton<Class>;

public:
  static Class& get(void) noexcept(true);

protected:
  explicit Singleton(Class *ptr_instance) noexcept(true);
  ~Singleton() noexcept(true);

protected:
  static Class* m_instance;
};

template <typename Class>
Class* Singleton<Class>::m_instance = nullptr;

template <typename Class>
Singleton<Class>::Singleton(Class *instance) noexcept(true)
{
  zv_assert_msg(m_instance == nullptr, "Constructor on singleton type called more than once!");
  m_instance = instance;
}

template <typename Class>
Singleton<Class>::~Singleton() noexcept(true)
{
  zv_assert_msg(m_instance != nullptr, "Destructor on singleton type called without constructor!");
}

template <typename Class>
Class &Singleton<Class>::get(void) noexcept(true)
{
  zv_assert_msg(m_instance != nullptr, "Singleton type hasn't been initialized!");
  return (*m_instance);
}

//------------------------------------------------------------------------------------------------------------------------------------
// FixedSizeString
//------------------------------------------------------------------------------------------------------------------------------------

template <size_t N>
class FixedSizeString
{
public:
  constexpr FixedSizeString()
    : m_length(0) 
  { 
    m_buffer[0] = '\0';
  }

  constexpr explicit FixedSizeString(const char* s)
    : m_length(0)
  {
    assign(s);
  }

  template <size_t M>
  constexpr explicit FixedSizeString(const char (&literal)[M])
    : m_length(0)
  {
    zv_assert_msg(M > 0, "literal includes null");
    zv_assert_msg(M - 1 <= N, "literal won't fit in FixedSizeString");

    for (size_t i = 0; i + 1 < M; ++i)
    {
      m_buffer[i] = literal[i];
    }

    m_length = M - 1;
    m_buffer[m_length] = '\0';
  }

  constexpr void assign(const char* s)
  {
    m_length = 0;

    while (s && s[m_length] != '\0' && m_length < N)
    {
      m_buffer[m_length] = s[m_length];
      ++m_length;
    }

    m_buffer[m_length] = '\0';
    
    zv_assert_msg(s && s[m_length] == '\0', "assign failed");
  }

  constexpr void append(const char* s)
  {
    size_t i = 0;
    while (s && s[i] != '\0' && m_length < N)
    {
      m_buffer[m_length++] = s[i++];
    }

    m_buffer[m_length] = '\0';

    zv_assert_msg(s && s[i] == '\0', "append failed");
  }

  constexpr void clear()
  {
    m_length = 0;
    m_buffer[0] = '\0';
  }

  // Observers
  constexpr const char* c_str() const { return m_buffer; }
  constexpr size_t size() const { return m_length; }
  constexpr size_t capacity() const { return N; }
  constexpr bool empty() const { return m_length == 0; }

  // Simple equality vs C string
  constexpr bool equals(const char* s) const
  {
    size_t i = 0;

    while (i < m_length && s && s[i] != '\0')
    {
      if (m_buffer[i] != s[i])
      {
        return false;
      }
      ++i;
    }

    return (i == m_length) && s && s[i] == '\0';
  }

private:
  char m_buffer[N + 1];
  size_t m_length;
};

//------------------------------------------------------------------------------------------------------------------------------------
// StringHash
//------------------------------------------------------------------------------------------------------------------------------------

class StringHash
{
private:
#if defined(ZV_ARCH_X64) || defined(ZV_ARCH_ARM64)
  static constexpr size_t k_val{ 0xcbf29ce484222325 };
  static constexpr size_t k_prime{ 0x100000001b3 };
#else
  static constexpr size_t k_val{ 0x811c9dc5 };
  static constexpr size_t k_prime{ 0x1000193 };
#endif

  static inline constexpr size_t hash_internal(const char* const str, const size_t value) noexcept
  {
    return (str[0] == '\0') ? value : hash_internal(&str[1], (value ^ size_t(str[0])) * k_prime);
  }

public:
  static inline constexpr StringHash hash(const char* const str) noexcept
  {
    return StringHash(hash_internal(str, k_val));
  }

  constexpr StringHash() noexcept
    : m_hash(0)
  {
  }

  explicit constexpr StringHash(const size_t hash) noexcept
    : m_hash(hash)
  {
  }

  explicit constexpr StringHash(const char* const ptr_text) noexcept
    : m_hash(hash_internal(ptr_text, k_val))
  {
  }

  // Support for hashing FixedSizeString
  template <size_t N>
  explicit constexpr StringHash(const FixedSizeString<N>& text) noexcept
    : m_hash(hash_internal(text.c_str(), k_val))
  {
  }

  // Support for hashing String (if present)
  explicit StringHash(const String& text)
    : m_hash(hash_internal(text.c_str(), k_val))
  {
  }

  inline constexpr operator size_t() const { return m_hash; }

  inline bool operator==(const StringHash& rhs) const { return m_hash == rhs.m_hash; }
  inline bool operator!=(const StringHash& rhs) const { return m_hash != rhs.m_hash; }
  inline bool operator<(const StringHash& rhs) const { return m_hash < rhs.m_hash; }
  inline bool operator>(const StringHash& rhs) const { return m_hash > rhs.m_hash; }

  inline constexpr size_t value() const { return m_hash; }

private:
  size_t m_hash;
};

template<>
struct std::hash<StringHash>
{
	[[nodiscard]] size_t operator()(const StringHash& hash) const noexcept
	{
    return hash.value();
	}
};

//------------------------------------------------------------------------------------------------------------------------------------
// Handedness flip utils
//------------------------------------------------------------------------------------------------------------------------------------

inline Matrix basis_flip_y(const Matrix& m)
{
  const Matrix b = Matrix{
     1,  0,  0,  0,
     0,  -1,  0,  0,
     0,  0,  1,  0,
     0,  0,  0,  1
  };
  return b * m * b;
}

inline Vector3 basis_flip_y(const Vector3& v)
{
  return Vector3{ v.x, -v.y, v.z };
}
