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


namespace input  {

	using InputClock = std::chrono::high_resolution_clock;
	using TimePoint = InputClock::time_point;
	
	class ActionSet
	{
	public:

		template< class ActionType >
		void add( ActionType&& action )
		{
			auto& action_set = find_or_create_set<ActionType>();
			action_set.add( std::forward<ActionType>( action ) );
		}

		template< class ActionType >
		bool contains( const ActionType& action ) const
		{
			if( auto* action_set = find_set<ActionType>() )
			{
				return action_set->contains( action );
			}
			return false;
		}

		void insert( const ActionSet& other )
		{
			for( const auto& set_pair : other.m_set_index )
			{
				if( auto action_set = find_set( set_pair.first ) )
				{
					action_set->insert( *set_pair.second );
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

		template< class ActionType >
		std::vector<ActionType> all_actions()
		{
			if( auto* action_set = find_set<ActionType>() )
			{
				return action_set->actions();
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
			boost::container::flat_set<T> m_actions;
		public:
			SetOf() = default;

			template< class Iterator >
			SetOf( Iterator&& it_begin, Iterator&& it_end ) 
				: m_actions( std::forward<Iterator>( it_begin ), std::forward<Iterator>( it_end ) ) 
			{}

			void add( T value ) { m_actions.emplace( std::move( value ) ); }
			bool contains( const T& value ) const { return m_actions.find( value ) != end(m_actions); }
			void insert( const Set& other ) override
			{ 
				const auto& specific_set = static_cast<const SetOf<T>&>( other );
				m_actions.insert( begin( specific_set.m_actions ), end( specific_set.m_actions ) );
			}

			std::unique_ptr<Set> clone() override
			{
				return std::make_unique<SetOf<T>>( begin( m_actions ), end( m_actions ) );
			}
			
			size_t size() const override { return m_actions.size(); }

			std::vector<T> actions() const
			{
				return { begin( m_actions ), end( m_actions ) };
			}
		};

		boost::container::flat_map<std::type_index, std::shared_ptr<Set>> m_set_index;

		Set* find_set( std::type_index type_id )
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
		SetOf<T>* find_set() { return static_cast<SetOf<T>*>( find_set( typeid(T) ) ); }

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

	class Condition;
	using ConditionSet = boost::container::flat_set<Condition>;

	enum class KeyId { ESCAPE, CTRL, ENTER, A, B, C, D, E, F, G, H, I, count };
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
			: m_actions( std::move( other.m_actions ) )
			, m_next_conditions( std::move( other.m_next_conditions ) )
			, m_predicate( std::move(other.m_predicate) )
		{
			std::cout << "CONDITION(&&)" << std::endl;
		}

		Condition& operator=( Condition&& other )
		{
			std::cout << "CONDITION=(&&)" << std::endl;
			m_actions = std::move( other.m_actions );
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


		ActionSet operator()( const InputUpdateInfo& update_info )
		{
			ActionSet actions_triggered;
			auto& predicate = *m_predicate;
			
			if( predicate( update_info ) )
			{
				actions_triggered = m_actions;

				for( auto& next_condition : m_next_conditions )
				{
					const auto triggered_actions = next_condition( update_info );
					actions_triggered.insert( triggered_actions );
				}
			}

			return actions_triggered;
		}


		template< class ActionType >
		void trigger( ActionType&& action ) { m_actions.add( std::forward<ActionType>( action ) ); }
		
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
			return m_next_conditions.size() + m_actions.size() < other.m_next_conditions.size() + other.m_actions.size();
		}

	private:
		
		struct Predicate 
		{
			virtual bool operator==( const Predicate& other ) const = 0;
			virtual bool operator()( const InputUpdateInfo& update_info ) = 0;
		};

		template< class Impl >
		struct PredicateImpl : public Predicate
		{
			Impl m_impl;

			PredicateImpl( Impl impl ) : m_impl( std::move( impl ) ) {}

			bool operator()( const InputUpdateInfo& update_info ) override 
			{
				return m_impl( update_info );
			}
			
			bool operator==( const Predicate& other ) const override
			{
				/*const auto other_name = typeid( other ).name();
				const auto this_name = typeid( PredicateImpl<Impl> ).name();
*/
				return typeid( other ) == typeid( *this )
					&& m_impl == static_cast<const PredicateImpl<Impl>&>(other).m_impl;
			}
		};

		std::unique_ptr<Predicate> m_predicate;
		ConditionSet m_next_conditions;
		ActionSet m_actions;

	};

	class KeyIsDown
	{
		KeyId m_key;
	public:
		explicit KeyIsDown( KeyId key_id ) : m_key( std::move(key_id) ) {}

		bool operator()( const InputUpdateInfo& info ) const
		{
			return info.keyboard().is_down( m_key );
		}

		bool operator==( const KeyIsDown& other ) const 
		{ 
			return m_key == other.m_key; 
		}
	};
	
	class InputMap
	{
	public:

		ActionSet operator()( const InputUpdateInfo& update_info )
		{
			ActionSet triggered_actions;
			for( auto& condition : m_root_conditions )
			{
				const auto additional_actions = condition( update_info );
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
		ConditionSet m_root_conditions;
	};


}


int main()
{
	using namespace input;
	input::InputMap input_map;
	
	input_map.on( KeyIsDown{ KeyId::ESCAPE } ).trigger( MyActions::ACTION_A );
	input_map.on( KeyIsDown{ KeyId::CTRL } ).trigger( MyActions::ACTION_B );
	input_map.on( KeyIsDown{ KeyId::CTRL } )
		.and_on( KeyIsDown{ KeyId::ENTER } )
		.trigger( MyActions::ACTION_C );

	InputUpdateInfo info;
	
	auto evaluate_input_map = [&] {
		auto triggered_actions = input_map( info );

		std::cout << "TRIGGERED ACTIONS: " << std::endl;
		/*for( const auto& action : triggered_actions )
		{
			std::cout << " - " << to_string(action) << std::endl;
		}*/
		for( const auto& action : triggered_actions.all_actions<MyActions>() )
		{
			std::cout << " - " << to_string(action) << std::endl;
		}
	};

	info.keyboard().push_down( KeyId::ESCAPE );
	evaluate_input_map();

	info.clear();
	info.keyboard().push_down( KeyId::CTRL );
	evaluate_input_map();

	info.clear();
	evaluate_input_map();

	info.clear();
	info.keyboard().push_down( KeyId::ESCAPE );
	evaluate_input_map();

	info.clear();
	info.keyboard().push_down( KeyId::ENTER );
	evaluate_input_map();

	info.keyboard().push_down( KeyId::CTRL );
	evaluate_input_map();

	info.keyboard().push_down( KeyId::ESCAPE );
	evaluate_input_map();

	info.keyboard().release( KeyId::CTRL );
	evaluate_input_map();


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




