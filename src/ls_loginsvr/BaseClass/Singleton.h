#pragma once
#include <memory>

template< typename T >
class Singleton
{
private:
	~Singleton() { };
	Singleton() { };
	Singleton( const T& other ) { };
	T& operator=( const T& other ) { };

public:
	inline static T* instance();

	static std::shared_ptr< T > pInstance;
};

template< typename T >
std::shared_ptr< T > Singleton< T >::pInstance;

template< typename T >
T* Singleton< T >::instance()
{
	if( 0 == pInstance )
		pInstance.reset( new T );

	return pInstance.get();
}     

