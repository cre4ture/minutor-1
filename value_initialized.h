#ifndef VALUE_INITIALIZED_H
#define VALUE_INITIALIZED_H

template<class T>
class value_initialized
{
  public :
    value_initialized() : x() {}
    operator T const &() const { return x ; }
    operator T&() { return x ; }
    value_initialized<T> operator=(const T& o) { x = o; return *this; }
  private :
    T x ;
} ;

#endif // VALUE_INITIALIZED_H
