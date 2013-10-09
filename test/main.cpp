#include <iostream>

#include "conditionmap.hpp"

enum MyActions
{
	ACTION_A
	, ACTION_B
	, ACTION_C
};

enum SpecialActions
{
	SPECIAL_A
	, SPECIAL_B
	, SPECIAL_C
};

enum MyModes
{
	MODE_X
	, MODE_Y
	, MODE_Z
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
	case( MyModes::MODE_Z ) : return "MODE_Z";
	default: return "UNKNOWN_MODE";
	}
}

enum class KeyId { ESCAPE, CTRL, ENTER, A, B, C, D, E, F, G, H, I, X, Y, count };
enum class KeyState { UP, DOWN };


class InputUpdateInfo
{
public:

	class Keyboard
	{
		std::array<KeyState, static_cast<size_t>( KeyId::count )> m_keystates;

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

	bool operator()( const InputUpdateInfo& info, const input::FlagBag& flags ) const
	{
		return flags.is_active( m_mode );
	}

	bool operator==( const IsModeActive& other ) const
	{
		return m_mode == other.m_mode;
	}
};



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

	mode_map.on( KeyIsDown{ KeyId::I } ).activate( MyModes::MODE_Z );

	InputUpdateInfo info;

	auto evaluate_mapping = [&] {
		auto mode_flags = mode_map( info );
		mode_flags.remove( MyModes::MODE_Z );

		const auto active_flags = input_map( info, mode_flags );

		std::cout << "TRIGGERED MODES: " << std::endl;
		for( const auto& mode : mode_flags.all<MyModes>() )
		{
			std::cout << " - " << to_string( mode ) << std::endl;
		}

		std::cout << "TRIGGERED ACTIONS: " << std::endl;
		for( const auto& action : active_flags.all<MyActions>() )
		{
			std::cout << " - " << to_string( action ) << std::endl;
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

	info.keyboard().release( KeyId::X );
	evaluate_mapping();

	info.keyboard().push_down( KeyId::I );
	evaluate_mapping();

	std::cin.ignore();
}




