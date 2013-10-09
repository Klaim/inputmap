#pragma once

#include <memory>
#include <typeindex>
#include <vector>

#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>

namespace input  {

	
	class FlagBag
	{
	public:

		template< class FlagType >
		void add( FlagType&& flag_value )
		{
			auto& flag_set = find_or_create_set<FlagType>();
			flag_set.add( std::forward<FlagType>( flag_value ) );
		}

		template< class FlagType >
		void remove( FlagType&& flag_value )
		{
			if( auto* flag_set = find_set<FlagType>() )
			{
				return flag_set->remove( flag_value );
			}
		}

		template< class FlagType >
		bool is_active( const FlagType& flag_value ) const
		{
			if( auto* flag_set = find_set<FlagType>() )
			{
				return flag_set->contains( flag_value );
			}
			return false;
		}
		
		void insert( const FlagBag& other )
		{
			for( const auto& set_pair : other.m_set_index )
			{
				if( auto flag_set = find_set( set_pair.first ) )
				{
					flag_set->insert( *set_pair.second );
				}
				else
				{
					m_set_index.emplace( set_pair.first, set_pair.second->clone() );
				}
			}
		}

		size_t size() const 
		{ 
			size_t total = 0;
			for( const auto& set_pair : m_set_index )
			{
				total += set_pair.second->size();
			}
			return total;
		}

		template< class FlagType >
		std::vector<FlagType> all() const
		{
			if( auto* flag_set = find_set<FlagType>() )
			{
				return flag_set->values();
			}
			return{};
		}


	private:

		struct Set 
		{
			virtual void insert( const Set& other ) = 0;
			virtual std::unique_ptr<Set> clone() = 0;
			virtual size_t size() const = 0;
		};
		
		template< class T > 
		class SetOf : public Set
		{
			boost::container::flat_set<T> m_values;
		public:
			SetOf() = default;

			template< class Iterator >
			SetOf( Iterator&& it_begin, Iterator&& it_end ) 
				: m_values( std::forward<Iterator>( it_begin ), std::forward<Iterator>( it_end ) ) 
			{}

			void add( T value ) { m_values.emplace( std::move( value ) ); }
			void remove( T value ) { m_values.erase( std::move( value ) ); }

			bool contains( const T& value ) const { return m_values.find( value ) != end(m_values); }
			void insert( const Set& other ) override
			{ 
				const auto& specific_set = static_cast<const SetOf<T>&>( other );
				m_values.insert( begin( specific_set.m_values ), end( specific_set.m_values ) );
			}

			std::unique_ptr<Set> clone() override
			{
				return std::make_unique<SetOf<T>>( begin( m_values ), end( m_values ) );
			}
			
			size_t size() const override { return m_values.size(); }

			std::vector<T> values() const
			{
				return { begin( m_values ), end( m_values ) };
			}
		};

		boost::container::flat_map<std::type_index, std::shared_ptr<Set>> m_set_index;

		Set* find_set( std::type_index type_id ) const
		{
			auto find_it = m_set_index.find( type_id );
			if( find_it != end( m_set_index ) )
			{
				auto& specific_set = *find_it->second;
				return &specific_set;
			}
			return nullptr;
		}

		
		template< class T >
		SetOf<T>* find_set() const { return static_cast<SetOf<T>*>( find_set( typeid(T) ) ); }

		template< class T >
		SetOf<T>& find_or_create_set()
		{
			if( auto* specific_set = find_set<T>() )
			{
				return *specific_set;
			}

			auto position = m_set_index.emplace( typeid(T), std::make_shared<SetOf<T>>() ).first;
			return static_cast<SetOf<T>&>(*position->second);
		}

	};

}
