#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <functional>
#include <typeindex>
#include <chrono>

#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>

enum MyActions
{
	ACTION_A
,	ACTION_B
,	ACTION_C
};

enum SpecialActions
{
	SPECIAL_A
,   SPECIAL_B
,   SPECIAL_C
};

enum MyModes
{
	MODE_X
,	MODE_Y
};

std::string to_string( const MyActions action )
{
	switch( action )
	{
	case( MyActions::ACTION_A ) : return "ACTION_A";
	case( MyActions::ACTION_B ) : return "ACTION_B";
	case( MyActions::ACTION_C ) : return "ACTION_C";
	default: return "UNKNOWN_ACTION";
	}
}

std::string to_string( const SpecialActions action )
{
	switch( action )
	{
	case( SpecialActions::SPECIAL_A ) : return "SPECIAL_A";
	case( SpecialActions::SPECIAL_B ) : return "SPECIAL_B";
	case( SpecialActions::SPECIAL_C ) : return "SPECIAL_C";
	default: return "UNKNOWN_SPECIAL";
	}
}

std::string to_string( const MyModes mode )
{
	switch( mode )
	{
	case( MyModes::MODE_X ) : return "MODE_X";
	case( MyModes::MODE_Y ) : return "MODE_Y";
	default: return "UNKNOWN_MODE";
	}
}


namespace input  {

	using InputClock = std::chrono::high_resolution_clock;
	using TimePoint = InputClock::time_point;
	
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

	template< class... UpdateArgs > class Condition;
	template< class... UpdateArgs >
	using ConditionSet = boost::container::flat_set<Condition<UpdateArgs...>>;

	template< class FlagType >
	using FlagSet = boost::container::flat_set<FlagType>;

	enum class KeyId { ESCAPE, CTRL, ENTER, A, B, C, D, E, F, G, H, I, X, Y, count };
	enum class KeyState { UP, DOWN };

	class InputUpdateInfo 
	{
	public:

		class Keyboard
		{
			std::array<KeyState, static_cast<size_t>(KeyId::count)> m_keystates;

		public:
			Keyboard() { clear(); }

			void push_down( KeyId key_id ) { m_keystates[ static_cast<size_t>( key_id ) ] = KeyState::DOWN; }
			void release( KeyId key_id ) { m_keystates[ static_cast<size_t>( key_id ) ] = KeyState::UP; }

			bool is_down( KeyId key_id ) const { return m_keystates[ static_cast<size_t>( key_id ) ] == KeyState::DOWN; }

			void clear() { m_keystates.assign( KeyState::UP ); }
		};

		const Keyboard& keyboard() const { return m_keyboard; }
		Keyboard& keyboard() { return m_keyboard; }


		void clear() { m_keyboard.clear(); }

	private:

		Keyboard m_keyboard;

	};

	template< class... UpdateArgs >
	class Condition
	{
	public:
		using Duration = std::chrono::microseconds;
		
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

	class KeyIsDown
	{
		KeyId m_key;
	public:
		explicit KeyIsDown( KeyId key_id ) : m_key( std::move( key_id ) ) {}

		template< class... Blahblah >
		bool operator()( const InputUpdateInfo& info, const Blahblah&... ) const
		{
			return info.keyboard().is_down( m_key );
		}

		bool operator==( const KeyIsDown& other ) const
		{
			return m_key == other.m_key;
		}
	};

	class IsModeActive
	{
		MyModes m_mode;
	public:
		explicit IsModeActive( MyModes mode ) : m_mode( std::move( mode ) ) {}

		bool operator()( const InputUpdateInfo& info, const FlagBag& flags ) const
		{
			return flags.is_active( m_mode );
		}

		bool operator==( const IsModeActive& other ) const
		{
			return m_mode == other.m_mode;
		}
	};

}


int main()
{
	using namespace input;
	input::ConditionMap<InputUpdateInfo> mode_map;
	input::ConditionMap<InputUpdateInfo, FlagBag> input_map;
		
	input_map.on( KeyIsDown{ KeyId::ESCAPE } ).activate( MyActions::ACTION_A );
	input_map.on( KeyIsDown{ KeyId::CTRL } ).activate( MyActions::ACTION_B );
	input_map.on( KeyIsDown{ KeyId::CTRL } )
		.and_on( KeyIsDown{ KeyId::ENTER } )
		.activate( MyActions::ACTION_C );
	
	input_map.on( KeyIsDown{ KeyId::CTRL } )
		.and_on( KeyIsDown{ KeyId::F } )
		.activate( SpecialActions::SPECIAL_A );

	input_map.on( IsModeActive{ MyModes::MODE_X } )
		.activate( SpecialActions::SPECIAL_B );
	
	mode_map.on( KeyIsDown{ KeyId::X } ).activate( MyModes::MODE_X );
	mode_map.on( KeyIsDown{ KeyId::Y } ).activate( MyModes::MODE_Y );

	InputUpdateInfo info;
	
	auto evaluate_mapping = [&] {
		const auto mode_flags = mode_map( info );
		const auto active_flags = input_map( info, mode_flags );

		std::cout << "TRIGGERED MODES: " << std::endl;
		for( const auto& mode : mode_flags.all<MyModes>() )
		{
			std::cout << " - " << to_string( mode ) << std::endl;
		}
		
		std::cout << "TRIGGERED ACTIONS: " << std::endl;
		for( const auto& action : active_flags.all<MyActions>() )
		{
			std::cout << " - " << to_string(action) << std::endl;
		}

		std::cout << "TRIGGERED SPECIAL: " << std::endl;
		for( const auto& action : active_flags.all<SpecialActions>() )
		{
			std::cout << " - " << to_string( action ) << std::endl;
		}
		std::cout << "----------------------------------" << std::endl;
	};

	info.keyboard().push_down( KeyId::ESCAPE );
	evaluate_mapping();

	info.clear();
	info.keyboard().push_down( KeyId::CTRL );
	evaluate_mapping();

	info.clear();
	evaluate_mapping();

	info.clear();
	info.keyboard().push_down( KeyId::ESCAPE );
	evaluate_mapping();

	info.clear();
	info.keyboard().push_down( KeyId::ENTER );
	evaluate_mapping();

	info.keyboard().push_down( KeyId::CTRL );
	evaluate_mapping();

	info.keyboard().push_down( KeyId::ESCAPE );
	evaluate_mapping();

	info.keyboard().release( KeyId::CTRL );
	evaluate_mapping();

	info.keyboard().clear();
	info.keyboard().push_down( KeyId::CTRL );
	info.keyboard().push_down( KeyId::F );
	evaluate_mapping();

	info.keyboard().push_down( KeyId::X );
	evaluate_mapping();


	/*input_map.on( KeyDown( Key_Ctrl ) ).and( KeyUp( Key_T ) ).activate_mode( "TextMode");
	input_map.on( KeyDown( Key_Ctrl ) ).and( KeyUp( Key_X ) ).deactivate_mode( "TextMode" );
	input_map.on( KeyDown( Key_Ctrl ) ).and( KeyUp( Key_C ) ).hold_mode( "Camera" );

	input_map.on_mode( "TextMode" ).and( MouseOver( "SomeId" ) ).action( MyActions::ACTION_A );
	*/

	/*Condition<MyActions> a{ KeyIsDown{ KeyId::ESCAPE } };
	Condition<MyActions> b{ KeyIsDown{ KeyId::ESCAPE } };
	Condition<MyActions> c{ KeyIsDown{ KeyId::CTRL } };
	
	if( a == b )
	{
		std::cout << "CORRECT" << std::endl;
	}
	else
	{
		std::cout << "NOT CORRECT" << std::endl;
	}

	if( a == c )
	{
		std::cout << "NOT CORRECT" << std::endl;
	}
	else
	{
		std::cout << "CORRECT" << std::endl;
	}

	if( c == KeyIsDown{ KeyId::CTRL } )
	{
		std::cout << "PASS" << std::endl;
	}
	else
	{
		std::cout << "DO NOT PASS" << std::endl;
	}

	if( c == KeyIsDown{ KeyId::ESCAPE } )
	{
		std::cout << "NOT CORRECT" << std::endl;
	}
	else
	{
		std::cout << "CORRECT" << std::endl;
	}
*/
	//const auto key_down = "[%key%]";
	//const auto key_up = "(%key)";
	//const auto key_just_down = "<%key%>";
	//const auto key_just_up = "{%key%}";

	//const auto and_ = " + ";
	//const auto or_ = " OR ";
	//
	//const auto a = "[CTRL] + <B> OR [CTRL] + <C>";
	//const auto b = "[MOUSE_LEFT]";

	std::cin.ignore();
}




