#pragma once

#include <memory>
#include <typeindex>

#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>

#include "bag.hpp"

namespace input  {

	template< class... UpdateArgs > class Condition;
	template< class... UpdateArgs >
	using ConditionSet = boost::container::flat_set<Condition<UpdateArgs...>>;

	template< class FlagType >
	using FlagSet = boost::container::flat_set<FlagType>;

	template< class... UpdateArgs >
	class Condition
	{
	public:
		
		template< class PredicateType >
		Condition( PredicateType&& predicate )
			: m_predicate( std::make_unique<PredicateImpl< PredicateType >>( std::forward<PredicateType>( predicate ) ) )
		{
			std::cout << "CONDITION() <" << typeid(PredicateType).name() << '>' << std::endl;
		}

		Condition( Condition&& other )
			: m_flags( std::move( other.m_flags ) )
			, m_next_conditions( std::move( other.m_next_conditions ) )
			, m_predicate( std::move(other.m_predicate) )
		{
			std::cout << "CONDITION(&&)" << std::endl;
		}

		Condition& operator=( Condition&& other )
		{
			std::cout << "CONDITION=(&&)" << std::endl;
			m_flags = std::move( other.m_flags );
			m_next_conditions = std::move( other.m_next_conditions );
			m_predicate = std::move( other.m_predicate );
			return *this;
		}

		Condition( const Condition& ) = delete;
		Condition& operator=( const Condition& ) = delete;

		friend inline bool operator==( const Condition& a, const Condition& b )
		{
			return *a.m_predicate == *b.m_predicate;
		}

		template< class T >
		friend inline bool operator==( const Condition& a, const T& b )
		{
			auto& predicate = *a.m_predicate;
			return typeid(predicate) == typeid( PredicateImpl<T> )
				&& static_cast<PredicateImpl<T>&>(predicate).m_impl == b;
		}


		FlagBag operator()( const UpdateArgs&... update_info )
		{
			FlagBag flags_triggered;
			auto& predicate = *m_predicate;
			
			if( predicate( update_info... ) )
			{
				flags_triggered = m_flags;

				for( auto& next_condition : m_next_conditions )
				{
					const auto triggered_flags = next_condition( update_info... );
					flags_triggered.insert( triggered_flags );
				}
			}

			return flags_triggered;
		}


		template< class FlagType >
		void activate( FlagType&& flag ) { m_flags.add( std::forward<FlagType>( flag ) ); }
		
		template< class PredicateType >
		Condition& and_on( PredicateType&& predicate )
		{
			for( auto& condition : m_next_conditions )
			{
				if( condition == predicate )
				{
					return condition;
				}
			}
			auto condition_it = m_next_conditions.emplace( std::forward<PredicateType>( predicate ) ).first;
			return *condition_it;
		}

		bool operator<( const Condition& other ) const
		{
			return m_next_conditions.size() + m_flags.size() < other.m_next_conditions.size() + other.m_flags.size();
		}

	private:
		
		struct Predicate 
		{
			virtual bool operator==( const Predicate& other ) const = 0;
			virtual bool operator()( const UpdateArgs&... update_info ) = 0;
		};

		template< class Impl >
		struct PredicateImpl : public Predicate
		{
			Impl m_impl;

			PredicateImpl( Impl impl ) : m_impl( std::move( impl ) ) {}

			bool operator()( const UpdateArgs&... update_info ) override 
			{
				return m_impl( update_info... );
			}
			
			bool operator==( const Predicate& other ) const override
			{
				return typeid( other ) == typeid( *this )
					&& m_impl == static_cast<const PredicateImpl<Impl>&>(other).m_impl;
			}
		};

		std::unique_ptr<Predicate> m_predicate;
		ConditionSet<UpdateArgs...> m_next_conditions;
		FlagBag m_flags;

	};

	
	template< class... UpdateArgs >
	class ConditionMap
	{
	public:
		using Condition = Condition<UpdateArgs...>;

		FlagBag operator()( const UpdateArgs&... update_info )
		{
			FlagBag triggered_actions;
			for( auto& condition : m_root_conditions )
			{
				const auto additional_actions = condition( update_info... );
				triggered_actions.insert( additional_actions );
			}
			return triggered_actions;
		}

		template< class PredicateType >
		Condition& on( PredicateType&& predicate )
		{
			for( auto& condition : m_root_conditions )
			{
				if( condition == predicate )
				{
					return condition;
				}
			}
			auto condition_it = m_root_conditions.emplace( std::forward<PredicateType>(predicate) ).first;
			return *condition_it;
		}
				
	private:
		ConditionSet<UpdateArgs...> m_root_conditions;
		
	};


}

