#ifndef STRUCTUTILS_HH
#define STRUCTUTILS_HH

// Generic member less than
template <typename T, typename M, typename C> struct member_lt_type {
  typedef M T::* member_ptr;
  member_lt_type( member_ptr p, C c ) : ptr(p), cmp(c) {}
  bool operator()( T const & lhs, T const & rhs ) const {
    return cmp( lhs.*ptr, rhs.*ptr );
  }
  member_ptr ptr;
  C cmp;
};

// dereference adaptor
template <typename T, typename C> struct dereferrer {
  dereferrer( C cmp ) : cmp(cmp) {}
  bool operator()( T * lhs, T * rhs ) const {
    return cmp( *lhs, *rhs );
  }
  C cmp;
};

// syntactic sugar
template <typename T, typename M> member_lt_type<T,M, std::less<M> > member_lt( M T::*ptr ) {
  return member_lt_type<T,M, std::less<M> >(ptr, std::less<M>() );
}

template <typename T, typename M, typename C> member_lt_type<T,M,C> member_lt( M T::*ptr, C cmp ) {
  return member_lt_type<T,M,C>( ptr, cmp );
}

template <typename T, typename C> dereferrer<T,C> deref( C cmp ) {
  return dereferrer<T,C>( cmp );
}

/*
// usage:    
struct test { int x; }
int main() {
   std::vector<test> v;
   std::sort( v.begin(), v.end(), member_lt( &test::x ) );
   std::sort( v.begin(), v.end(), member_lt( &test::x, std::greater<int>() ) );

   std::vector<test*> vp;
   std::sort( v.begin(), v.end(), deref<test>( member_lt( &test::x ) ) );
}
*/
#endif
