#include "movement.h"
#include "../../game/sdk/includes/includes.h"
#include "../../globals/includes/includes.h"
#include "../prediction/prediction.h"

namespace
{
	[[nodiscard]] __forceinline bool is_in_game_and_connected( )
	{
		return g_interfaces.m_engine_client->is_connected( ) && g_interfaces.m_engine_client->is_in_game( );
	}

	[[nodiscard]] __forceinline bool founded_local_entity( )
	{
		return g_ctx.m_local;
	}
	
	[[nodiscard]] __forceinline bool founded_command_line( )
	{
		return g_ctx.m_cmd;
	}

	[[nodiscard]] __forceinline bool local_entity_is_alive( )
	{
		return founded_local_entity( ) && g_ctx.m_local->get_observer_mode( ) <= 0 && g_ctx.m_local->is_alive( );
	}

	[[nodiscard]] __forceinline bool allowed_proccess( )
	{
		return founded_command_line( ) && is_in_game_and_connected( ) && local_entity_is_alive( );
	}

	[[nodiscard]] __forceinline bool is_feature_active( const bool enabled, key_bind_t& key )
	{
		return enabled && g_input.check_input( &key );
	}

	[[nodiscard]] __forceinline bool has_invalid_movement_state( )
	{
		return g_utilities.is_in< int >( g_ctx.m_local->get_flags( ), invalid_flags ) ||
		       g_utilities.is_in< int >( g_prediction.backup_data.m_flags, invalid_flags ) ||
		       g_utilities.is_in< int >( g_ctx.m_local->get_move_type( ), invalid_move_types ) ||
		       g_utilities.is_in< int >( g_prediction.backup_data.m_move_type, invalid_move_types );
	}

	[[nodiscard]] __forceinline bool local_entity_is_not_falling( )
	{
		return std::roundf( g_prediction.backup_data.m_velocity.m_z ) >= 0.f;
	}
	
	[[nodiscard]] __forceinline bool local_entity_is_staying( )
	{
		return std::roundf( g_ctx.m_local->get_velocity( ).m_z ) == 0.f;
	}

	__forceinline void predict_cmd( c_user_cmd* cmd )
	{
		g_prediction.begin( g_ctx.m_local, cmd );
		g_prediction.end( g_ctx.m_local );
	}

	__forceinline void restore_prediction_frame( )
	{
		g_prediction.restore_entity_to_predicted_frame( g_interfaces.m_prediction->m_commands_predicted - 1 );
	}

	__forceinline bool is_pressed_button( c_user_cmd* cmd, e_command_buttons buttons )
	{
		return cmd->m_buttons & ( buttons );
	}

	__forceinline void add_button( c_user_cmd* cmd, e_command_buttons buttons )
	{
		cmd->m_buttons |= ( buttons );
	}

	__forceinline void clear_button( c_user_cmd* cmd, e_command_buttons buttons )
	{
		cmd->m_buttons &= ~( buttons );
	}

	__forceinline void stop_forward_and_back_move( c_user_cmd* cmd )
	{
		cmd->m_forward_move = 0.f;

		clear_button( cmd, in_forward );
		clear_button( cmd, in_back );
	}

	__forceinline void stop_full_move( c_user_cmd* cmd )
	{
		cmd->m_forward_move = 0.f;
		cmd->m_side_move    = 0.f;

		clear_button( cmd, in_forward );
		clear_button( cmd, in_back );
		clear_button( cmd, in_moveleft );
		clear_button( cmd, in_moveright );
	}

	__forceinline void clear_move_buttons( c_user_cmd* cmd )
	{
		clear_button( cmd, in_forward );
		clear_button( cmd, in_back );
		clear_button( cmd, in_moveleft );
		clear_button( cmd, in_moveright );
	}
} // namespace

void n_movement::impl_t::on_create_move_pre( )
{
	if ( !allowed_proccess( ) )
		return;

	bunny_hop( );
	infinity_duck( );
}

void n_movement::impl_t::on_create_move_post( )
{
	if ( !allowed_proccess( ) )
		return;

	edge_jump( );
	long_jump( );
	mini_jump( );

	auto_align( );
	pixel_surf_fix( );

	jump_bug( );
	jump_bug_simulation( );
	pixel_surf( );
	auto_duck( );

	movement_fix( g_prediction.backup_data.m_view_angles );
	edge_bug( );
}

void n_movement::impl_t::bunny_hop( )
{
	if ( !GET_VARIABLE( g_variables.m_bunny_hop, bool ) )
		return;

	if ( is_feature_active( GET_VARIABLE( g_variables.m_jump_bug, bool ), GET_VARIABLE( g_variables.m_jump_bug_key, key_bind_t ) ) )
		return;

	const int move_type        = g_ctx.m_local->get_move_type( );
	const int backup_move_type = g_prediction.backup_data.m_move_type;

	if ( move_type == e_move_types::move_type_ladder || backup_move_type == e_move_types::move_type_ladder )
		return;

	if ( !( g_ctx.m_local->get_flags( ) & e_flags::fl_onground ) && is_pressed_button( g_ctx.m_cmd, in_jump ) )
		clear_button( g_ctx.m_cmd, in_jump );
}

void n_movement::impl_t::infinity_duck( )
{
	if ( GET_VARIABLE( g_variables.m_no_crouch_cooldown, bool ) )
		add_button( g_ctx.m_cmd, in_bullrush );
}

void n_movement::impl_t::edge_jump( )
{
	static int saved_ticks = 0;

	if ( !is_feature_active( GET_VARIABLE( g_variables.m_edge_jump, bool ), GET_VARIABLE( g_variables.m_edge_jump_key, key_bind_t ) ) ) {
		saved_ticks = 0;
		return;
	}

	if ( g_ctx.m_local->get_flags( ) & e_flags::fl_onground )
		return;

	if ( g_prediction.backup_data.m_flags & e_flags::fl_onground )
		add_button( g_ctx.m_cmd, in_jump );

	if ( GET_VARIABLE( g_variables.m_edge_jump_ladder, bool ) ) {
		const auto base_move_type = g_ctx.m_local->get_move_type( );

		predict_cmd( g_ctx.m_cmd );

		if ( base_move_type == e_move_types::move_type_ladder && g_ctx.m_local->get_move_type( ) != e_move_types::move_type_ladder ) {
			saved_ticks = g_interfaces.m_global_vars_base->m_tick_count;

			add_button( g_ctx.m_cmd, in_jump );
			stop_full_move( g_ctx.m_cmd );
		}

		const int delta = g_interfaces.m_global_vars_base->m_tick_count - saved_ticks;

		if ( delta > 3 && delta < 15 ) {
			stop_full_move( g_ctx.m_cmd );
			add_button( g_ctx.m_cmd, in_duck );
		}
	}
}

void n_movement::impl_t::pixel_surf_fix( )
{
	if ( !GET_VARIABLE( g_variables.m_pixel_surf_fix, bool ) )
		return;

	static const auto sv_airaccelerate = g_convars[ HASH_BT( "sv_airaccelerate" ) ];

	const auto velocity = g_ctx.m_local->get_velocity( );

	if ( velocity.m_z >= 0.f )
		return;

	if ( !( g_ctx.m_local->get_flags( ) & fl_onground ) )
		return;

	const float tickrate  = 1.f / g_interfaces.m_global_vars_base->m_interval_per_tick;
	const float wishdelta = ( velocity.length_2d( ) + 2.f - 285.91f ) / sv_airaccelerate->get_float( ) * tickrate;

	const auto velo_ang  = c_vector( velocity * -1.f ).to_angle( ).normalize( );
	const float rotation = deg2rad( velo_ang.m_y - g_prediction.backup_data.m_view_angles.m_y );

	g_ctx.m_cmd->m_forward_move = std::cosf( rotation ) * wishdelta;
	g_ctx.m_cmd->m_side_move    = -std::sinf( rotation ) * wishdelta;
}

void n_movement::impl_t::edge_bug( )
{
	if ( !is_feature_active( GET_VARIABLE( g_variables.m_edge_bug, bool ), GET_VARIABLE( g_variables.m_edge_bug_key, key_bind_t ) ) ) {
		m_edgebug_data.reset( );
		return;
	}

	if ( m_jumpbug_data.m_will_should || m_pixelsurf_data.m_detected ) {
		m_edgebug_data.reset( );
		return;
	}

	const float yaw_delta = std::clamp( g_prediction.backup_data.m_view_angles.m_y - g_ctx.m_last_tick_yaw,
	                -( 180.f / GET_VARIABLE( g_variables.m_edge_bug_ticks, int ) ), ( 180.f / GET_VARIABLE( g_variables.m_edge_bug_ticks, int ) ) );

	const float original_forward_move = g_ctx.m_cmd->m_forward_move;
	const float original_side_move    = g_ctx.m_cmd->m_side_move;
	const auto original_view_point    = g_ctx.m_cmd->m_view_point;

	const auto loop_through_ticks = [ & ]( const bool ducked, const bool strafe ) {
		if ( m_edgebug_data.m_will_edgebug )
			return;

		restore_prediction_frame( );
		m_edgebug_data.m_starting_yaw = original_view_point.m_y;

		for ( int i = 0; i <= GET_VARIABLE( g_variables.m_edge_bug_ticks, int ); ++i ) {
			c_user_cmd simulated_cmd = *g_ctx.m_cmd;
			add_button( &simulated_cmd, in_bullrush );

			if ( ducked ) {
				if ( is_pressed_button( &simulated_cmd, in_jump ) )
					clear_button( &simulated_cmd, in_jump );

				add_button( &simulated_cmd, in_duck );
			}
			else 
				clear_button( &simulated_cmd, in_duck );

			if ( !strafe ) {
				m_edgebug_data.m_strafing    = false;
				stop_full_move( &simulated_cmd );
			} else {
				m_edgebug_data.m_strafing      = true;
				simulated_cmd.m_forward_move   = original_forward_move;
				simulated_cmd.m_side_move      = original_side_move;
				simulated_cmd.m_view_point.m_y = g_math.normalize_angle( original_view_point.m_y + ( yaw_delta * i ) );
			}

			predict_cmd( &simulated_cmd );

			if ( has_invalid_movement_state( ) || local_entity_is_not_falling( ) || local_entity_is_staying( ) ) {
				m_edgebug_data.m_will_edgebug = false;
				break;
			}

			if ( !m_edgebug_data.m_will_edgebug )
				detect_edgebug( &simulated_cmd );

			if ( m_edgebug_data.m_will_edgebug ) {
				m_edgebug_data.m_saved_mousedx  = std::abs( simulated_cmd.m_mouse_delta_x );
				m_edgebug_data.m_ticks_to_stop  = i + 1;
				m_edgebug_data.m_last_tick      = g_interfaces.m_global_vars_base->m_tick_count;
				m_edgebug_data.m_edgebug_method = ducked ? edgebug_type_t::eb_ducking : edgebug_type_t::eb_standing;

				if ( strafe ) {
					m_edgebug_data.m_yaw_delta    = yaw_delta;
					m_edgebug_data.m_forward_move = original_forward_move;
					m_edgebug_data.m_side_move    = original_side_move;
				} else {
					m_edgebug_data.m_forward_move = 0.f;
					m_edgebug_data.m_side_move    = 0.f;
				}

				break;
			}

			if ( m_edgebug_data.m_will_fail ) {
				m_edgebug_data.m_will_fail = false;
				break;
			}
		}

		predict_cmd( g_ctx.m_cmd );
		restore_prediction_frame( );
	};

	loop_through_ticks( false, false );
	loop_through_ticks( true, false );

	if ( GET_VARIABLE( g_variables.m_advanced_detection, bool ) ) {
		loop_through_ticks( false, true );
		loop_through_ticks( true, true );
	}

	if ( !m_edgebug_data.m_will_edgebug )
		return;

	if ( g_interfaces.m_global_vars_base->m_tick_count < m_edgebug_data.m_ticks_to_stop + m_edgebug_data.m_last_tick + 1 ) {
		clear_move_buttons( g_ctx.m_cmd );

		if ( m_edgebug_data.m_strafing ) {
			g_ctx.m_cmd->m_side_move    = m_edgebug_data.m_side_move;
			g_ctx.m_cmd->m_forward_move = m_edgebug_data.m_forward_move;

			const float final_yaw = g_math.normalize_angle(
				m_edgebug_data.m_starting_yaw +
				( m_edgebug_data.m_yaw_delta * ( g_interfaces.m_global_vars_base->m_tick_count - m_edgebug_data.m_last_tick ) ) );

			g_ctx.m_cmd->m_view_point.m_y = final_yaw;
		} else {
			stop_full_move( g_ctx.m_cmd );
		}

		if ( m_edgebug_data.m_edgebug_method == edgebug_type_t::eb_ducking ) {
			if ( is_pressed_button( g_ctx.m_cmd, in_jump ) )
				clear_button( g_ctx.m_cmd, in_jump );

			add_button( g_ctx.m_cmd, in_duck );
		}
		else
			clear_button( g_ctx.m_cmd, in_duck );

	} else {
		m_edgebug_data.reset( );
	}
}

void n_movement::impl_t::long_jump( )
{
	static int saved_ticks = 0;

	if ( !is_feature_active( GET_VARIABLE( g_variables.m_long_jump, bool ), GET_VARIABLE( g_variables.m_long_jump_key, key_bind_t ) ) ) {
		saved_ticks = 0;
		return;
	}

	if ( g_ctx.m_local->get_flags( ) & e_flags::fl_onground )
		saved_ticks = g_interfaces.m_global_vars_base->m_tick_count;

	if ( ( g_interfaces.m_global_vars_base->m_tick_count - saved_ticks <= 2 ) && !( g_ctx.m_local->get_flags( ) & e_flags::fl_onground ) ) {
		stop_forward_and_back_move( g_ctx.m_cmd );
		add_button( g_ctx.m_cmd, in_duck );
	}
}

void n_movement::impl_t::mini_jump( )
{
	static bool should_duck = false;

	if ( !is_feature_active( GET_VARIABLE( g_variables.m_mini_jump, bool ), GET_VARIABLE( g_variables.m_mini_jump_key, key_bind_t ) ) ) {
		should_duck = false;
		return;
	}

	const int cur_flags  = g_ctx.m_local->get_flags( );
	const int prev_flags = g_prediction.backup_data.m_flags;

	if ( ( prev_flags & e_flags::fl_onground ) && ( cur_flags & e_flags::fl_onground ) )
		should_duck = false;

	if ( ( prev_flags & e_flags::fl_onground ) && !( cur_flags & e_flags::fl_onground ) ) {
		stop_forward_and_back_move( g_ctx.m_cmd );
		add_button( g_ctx.m_cmd, in_jump );
		add_button( g_ctx.m_cmd, in_duck );

		if ( GET_VARIABLE( g_variables.m_mini_jump_hold_duck, bool ) )
			should_duck = true;
	}

	if ( should_duck )
		add_button( g_ctx.m_cmd, in_duck );
}

void n_movement::impl_t::auto_duck( )
{
	if ( !GET_VARIABLE( g_variables.m_auto_duck, bool ) || ( g_prediction.backup_data.m_flags & e_flags::fl_onground ) ||
	     m_edgebug_data.m_will_edgebug || m_pixelsurf_data.m_detected ||
	     is_feature_active( GET_VARIABLE( g_variables.m_jump_bug, bool ), GET_VARIABLE( g_variables.m_jump_bug_key, key_bind_t ) ) ||
	     is_feature_active( GET_VARIABLE( g_variables.m_edge_jump, bool ), GET_VARIABLE( g_variables.m_edge_jump_key, key_bind_t ) ) ) {
		m_autoduck_data.reset( );
		return;
	}

	restore_prediction_frame( );

	for ( int i = 0; i < 2; ++i ) {
		if ( g_ctx.m_local->get_flags( ) & e_flags::fl_onground )
			break;

		c_user_cmd simulated_cmd = *g_ctx.m_cmd;
		add_button( &simulated_cmd, in_bullrush );
		add_button( &simulated_cmd, in_duck );

		predict_cmd( &simulated_cmd );

		if ( g_ctx.m_local->get_flags( ) & e_flags::fl_onground ) {
			g_movement.m_autoduck_data.m_did_land_ducking = true;
			g_movement.m_autoduck_data.m_ducking_vert     = g_ctx.m_local->get_abs_origin( ).m_z;
			break;
		}
	}

	predict_cmd( g_ctx.m_cmd );
	restore_prediction_frame( );

	if ( !g_movement.m_autoduck_data.m_did_land_ducking )
		return;

	for ( int i = 0; i < 2; ++i ) {
		if ( g_ctx.m_local->get_flags( ) & e_flags::fl_onground )
			break;

		c_user_cmd simulated_cmd = *g_ctx.m_cmd;
		clear_button( &simulated_cmd, in_bullrush );
		clear_button( &simulated_cmd, in_duck );

		predict_cmd( &simulated_cmd );

		if ( g_ctx.m_local->get_flags( ) & e_flags::fl_onground ) {
			g_movement.m_autoduck_data.m_did_land_standing = true;
			g_movement.m_autoduck_data.m_standing_vert     = g_ctx.m_local->get_abs_origin( ).m_z;
			break;
		}
	}

	predict_cmd( g_ctx.m_cmd );
	restore_prediction_frame( );

	if ( g_movement.m_autoduck_data.m_did_land_ducking && !g_movement.m_autoduck_data.m_did_land_standing )
		add_button( g_ctx.m_cmd, in_duck );
	else if ( g_movement.m_autoduck_data.m_did_land_ducking && g_movement.m_autoduck_data.m_did_land_standing &&
	          g_movement.m_autoduck_data.m_ducking_vert > g_movement.m_autoduck_data.m_standing_vert )
		add_button( g_ctx.m_cmd, in_duck );
}

void n_movement::impl_t::pixel_surf( )
{
	if ( m_pixelsurf_data.m_detected ) {
		add_button( g_ctx.m_cmd, in_bullrush );
		add_button( g_ctx.m_cmd, in_duck );
	}

	if ( !is_feature_active( GET_VARIABLE( g_variables.m_pixel_surf, bool ), GET_VARIABLE( g_variables.m_pixel_surf_key, key_bind_t ) ) ||
	     is_feature_active( GET_VARIABLE( g_variables.m_edge_bug, bool ), GET_VARIABLE( g_variables.m_edge_bug_key, key_bind_t ) ) ) {
		m_pixelsurf_data.m_detected    = false;
		return;
	}

	if ( has_invalid_movement_state( ) || local_entity_is_not_falling( ) || local_entity_is_staying( ) || m_jumpbug_data.m_will_should || m_edgebug_data.m_will_edgebug ) {
		m_pixelsurf_data.m_detected    = false;
		return;
	}

	for ( int i = 0; i <= 32; ++i ) {
		c_user_cmd simulated_cmd = *g_ctx.m_cmd;
		add_button( &simulated_cmd, in_bullrush );
		add_button( &simulated_cmd, in_duck );
		clear_button( &simulated_cmd, in_jump );

		const float prev_vel_z = g_ctx.m_local->get_velocity( ).m_z;
		const int prev_flags   = g_ctx.m_local->get_flags( );

		predict_cmd( &simulated_cmd );

		const float cur_vel_z = g_ctx.m_local->get_velocity( ).m_z;
		const int cur_flags   = g_ctx.m_local->get_flags( );

		if ( !( prev_flags & fl_onground ) && !( cur_flags & fl_onground ) ) {
			if ( prev_vel_z < g_ctx.half_gravity_per_tick && std::floorf( cur_vel_z ) > std::floorf( prev_vel_z ) && std::roundf(cur_vel_z) == std::roundf(g_ctx.half_gravity_per_tick) ) {
				m_pixelsurf_data.m_detected = std::roundf( prev_vel_z - g_ctx.half_gravity_per_tick * g_interfaces.m_global_vars_base->m_interval_per_tick ) < std::roundf( g_ctx.m_local->get_velocity( ).m_z );
				return;
			}
		}
	}
}

void n_movement::impl_t::jump_bug_simulation( )
{
	if ( !is_feature_active( GET_VARIABLE( g_variables.m_jump_bug, bool ), GET_VARIABLE( g_variables.m_jump_bug_key, key_bind_t ) ) ) {
		m_jumpbug_data.m_will_should = false;
		return;
	}

	if ( has_invalid_movement_state( ) || m_edgebug_data.m_will_edgebug || m_pixelsurf_data.m_detected ) {
		m_jumpbug_data.m_will_should = false;
		return;
	}

	for ( int i = 0; i <= 32; ++i ) {
		c_user_cmd simulated_cmd = *g_ctx.m_cmd;
		add_button( &simulated_cmd, in_bullrush );
		add_button( &simulated_cmd, in_duck );
		clear_button( &simulated_cmd, in_jump );

		const float prev_vel_z    = g_ctx.m_local->get_velocity( ).m_z;
		const int prev_flags      = g_ctx.m_local->get_flags( );

		predict_cmd( &simulated_cmd );

		const float cur_vel_z    = g_ctx.m_local->get_velocity( ).m_z;
		const int cur_flags      = g_ctx.m_local->get_flags( );

		if ( !( prev_flags & fl_onground ) && ( cur_flags & fl_onground ) ) {
			if ( prev_vel_z < std::roundf( g_ctx.half_gravity_per_tick ) && std::floorf( cur_vel_z ) > std::floorf( prev_vel_z ) &&
			     std::roundf( cur_vel_z ) > std::roundf( g_ctx.half_gravity_per_tick ) ) {
				m_jumpbug_data.m_will_should = std::roundf( prev_vel_z - g_ctx.half_gravity_per_tick * g_interfaces.m_global_vars_base->m_interval_per_tick ) <= std::roundf( g_ctx.m_local->get_velocity( ).m_z );
				return;
			}
		}
	}

	m_jumpbug_data.m_will_should = false;
}

void n_movement::impl_t::jump_bug( )
{
	if ( !is_feature_active( GET_VARIABLE( g_variables.m_jump_bug, bool ), GET_VARIABLE( g_variables.m_jump_bug_key, key_bind_t ) ) )
		return;

	static bool ducked = false;

	if ( !( is_pressed_button( g_ctx.m_cmd, in_jump ) ) ) {
		if ( ( g_ctx.m_local->get_flags( ) & e_flags::fl_onground ) && !( g_prediction.backup_data.m_flags & e_flags::fl_onground ) && !ducked ) {
			add_button( g_ctx.m_cmd, in_duck );
			ducked = true;
		} else {
			ducked = false;
		}

		if ( ( g_prediction.backup_data.m_flags & e_flags::fl_onground ) && ducked )
			ducked = false;

		return;
	}

	if ( ( g_ctx.m_local->get_flags( ) & e_flags::fl_onground ) && !( g_prediction.backup_data.m_flags & e_flags::fl_onground ) ) 
		add_button( g_ctx.m_cmd, in_duck );

	if ( g_ctx.m_local->get_flags( ) & e_flags::fl_onground )
		clear_button( g_ctx.m_cmd, in_jump );

	if ( !( g_ctx.m_local->get_flags( ) & fl_onground ) && ( g_prediction.backup_data.m_flags & fl_onground ) )
		clear_button( g_ctx.m_cmd, in_duck );
}

void n_movement::impl_t::auto_align( )
{
	if ( !GET_VARIABLE( g_variables.m_auto_align, bool ) )
		return;

	if ( has_invalid_movement_state( ) )
		return;

	static const float max_fw_move = g_convars[ HASH_BT( "cl_forwardspeed" ) ]->get_float( );
	static const float max_sw_move = g_convars[ HASH_BT( "cl_sidespeed" ) ]->get_float( );

	constexpr auto rotate = []( c_angle& angle, const float speed ) {
		const float rotation        = deg2rad( g_ctx.m_cmd->m_view_point.m_y - angle.m_y );
		g_ctx.m_cmd->m_forward_move = std::cosf( rotation ) * speed;
		g_ctx.m_cmd->m_side_move    = std::sinf( rotation ) * speed;
	};

	const c_vector origin      = g_ctx.m_local->get_abs_origin( );
	const c_vector velocity    = g_ctx.m_local->get_velocity( );
	const c_vector player_maxs = g_ctx.m_local->get_collideable( )->get_obb_maxs( );

	constexpr float distance_till_adjust = 0.03125f;
	constexpr float error_margin         = 0.01f;
	const float align_trace_additive     = error_margin + player_maxs.m_x;

	auto get_colliding_wall = [ & ]( trace_t& out_trace ) -> bool {
		const float fw_move = g_ctx.m_cmd->m_forward_move / max_fw_move;
		const float sw_move = g_ctx.m_cmd->m_side_move / max_sw_move;

		const c_vector va_forward = g_ctx.m_cmd->m_view_point.forward( ).normalize( ).to_vector( );
		const c_vector va_right   = g_ctx.m_cmd->m_view_point.right( ).normalize( ).to_vector( );

		const c_vector wish_dir = { va_forward.m_x * fw_move + va_right.m_x * sw_move, va_forward.m_y * fw_move + va_right.m_y * sw_move, 0.f };

		const c_vector direct_dir = { std::round( wish_dir.m_x ), std::round( wish_dir.m_y ), 0.f };

		const float trace_additive = ( distance_till_adjust + error_margin ) + player_maxs.m_x;
		const c_vector trace_dir   = origin + c_vector( trace_additive * direct_dir.m_x, trace_additive * direct_dir.m_y, 0.f );

		{
			trace_t al_trace{ };
			c_trace_filter al_filter( g_ctx.m_local );

			const c_vector align_trace_dir = origin + c_vector( align_trace_additive * direct_dir.m_x, align_trace_additive * direct_dir.m_y, 0.f );

			ray_t ray( origin, align_trace_dir );
			g_interfaces.m_engine_trace->trace_ray( ray, mask_playersolid, &al_filter, &al_trace );

			if ( al_trace.did_hit( ) || al_trace.m_plane.m_normal.m_z != 0.f )
				return false;
		}

		trace_t trace{ };
		c_trace_filter filter( g_ctx.m_local );
		ray_t ray( origin, trace_dir );
		g_interfaces.m_engine_trace->trace_ray( ray, mask_playersolid, &filter, &trace );

		if ( trace.m_plane.m_normal.m_z != 0.f )
			return false;

		if ( trace.did_hit( ) ) {
			out_trace = trace;
			return true;
		}

		return false;
	};

	trace_t hit_trace{ };

	if ( !get_colliding_wall( hit_trace ) )
		return;

	auto wall_angle   = hit_trace.m_plane.m_normal.to_angle( ).flip( );
	auto strafe_angle = c_angle( g_ctx.m_cmd->m_view_point.m_x, wall_angle.m_y, g_ctx.m_cmd->m_view_point.m_z );

	rotate( strafe_angle, 10.f );
}

void n_movement::impl_t::on_frame_stage_notify( int stage )
{
	if ( !g_movement.m_edgebug_data.m_will_edgebug || !g_movement.m_edgebug_data.m_strafing || stage != e_client_frame_stage::start ) {
		return;
	}

	c_angle wish_angles = { g_prediction.backup_data.m_view_angles.m_x, g_movement.m_edgebug_data.m_starting_yaw, 0.f };

	const float hit_time_delta = g_math.ticks_to_time( g_movement.m_edgebug_data.m_ticks_to_stop );
	const float cur_time_delta = g_interfaces.m_global_vars_base->m_current_time - g_math.ticks_to_time( g_movement.m_edgebug_data.m_last_tick );

	const float final_yaw = g_math.normalize_angle( g_movement.m_edgebug_data.m_yaw_delta *
	                                                ( g_movement.m_edgebug_data.m_ticks_to_stop * ( cur_time_delta / hit_time_delta ) ) );

	wish_angles.m_y += final_yaw;

	g_interfaces.m_engine_client->set_view_angles( wish_angles );
}

void n_movement::impl_t::detect_edgebug( c_user_cmd* cmd )
{
	if ( has_invalid_movement_state( ) || local_entity_is_not_falling( ) || local_entity_is_staying( ) || m_jumpbug_data.m_will_should ||
	     m_pixelsurf_data.m_detected ) {
		m_edgebug_data.m_will_edgebug = false;
		m_edgebug_data.m_will_fail    = true;
		return;
	}

	const auto gravity = g_convars[ HASH_BT( "sv_gravity" ) ]->get_float( );

	float gravity_vel = ( -( gravity * 0.5f ) * g_interfaces.m_global_vars_base->m_interval_per_tick );

	if ( g_prediction.backup_data.m_velocity.m_z < std::roundf( g_ctx.half_gravity_per_tick ) &&
	            std::floorf( g_ctx.m_local->get_velocity( ).m_z ) > std::floorf( g_prediction.backup_data.m_velocity.m_z ) &&
	            g_ctx.m_local->get_velocity( ).m_z < std::roundf( g_ctx.half_gravity_per_tick ) ) {
		const float previous_velocity = g_ctx.m_local->get_velocity( ).m_z;

		g_prediction.begin( g_ctx.m_local, cmd );
		g_prediction.end( g_ctx.m_local );

		const float expected_vertical_velocity = std::roundf( previous_velocity - gravity * g_interfaces.m_global_vars_base->m_interval_per_tick );

		m_edgebug_data.m_will_edgebug = expected_vertical_velocity == std::roundf( g_ctx.m_local->get_velocity( ).m_z );
		m_edgebug_data.m_will_fail    = !( expected_vertical_velocity == std::roundf( g_ctx.m_local->get_velocity( ).m_z ) );
	}
}

void n_movement::impl_t::edgebug_data_t::reset( )
{
	m_edgebug_method = edgebug_type_t::eb_standing;
	m_will_edgebug   = false;
	m_will_fail      = false;
	m_strafing       = false;
	m_yaw_delta      = 0.f;
	m_starting_yaw   = 0.f;
	m_side_move      = 0.f;
	m_forward_move   = 0.f;
	m_saved_mousedx  = 0.f;
	m_ticks_to_stop  = 0;
	m_last_tick      = 0;
}

void n_movement::impl_t::autoduck_data_t::reset( )
{
	m_did_land_ducking  = false;
	m_did_land_standing = false;
	m_ducking_vert      = 0.f;
	m_standing_vert     = 0.f;
}

void n_movement::impl_t::movement_fix( const c_angle& old_view_point )
{
	c_vector forward{ }, right{ }, up{ };
	g_math.angle_vectors( old_view_point, &forward, &right, &up );

	forward.m_z = 0.f;
	right.m_z   = 0.f;
	up.m_x      = 0.f;
	up.m_y      = 0.f;

	forward.normalize_in_place( );
	right.normalize_in_place( );
	up.normalize_in_place( );

	c_vector cmd_forward{ }, cmd_right{ }, cmd_up{ };
	g_math.angle_vectors( g_ctx.m_cmd->m_view_point, &cmd_forward, &cmd_right, &cmd_up );

	cmd_forward.m_z = 0.f;
	cmd_right.m_z   = 0.f;
	cmd_up.m_x      = 0.f;
	cmd_up.m_y      = 0.f;

	cmd_forward.normalize_in_place( );
	cmd_right.normalize_in_place( );
	cmd_up.normalize_in_place( );

	const float pitch_forward = forward.m_x * g_ctx.m_cmd->m_forward_move;
	const float yaw_forward   = forward.m_y * g_ctx.m_cmd->m_forward_move;
	const float pitch_side    = right.m_x * g_ctx.m_cmd->m_side_move;
	const float yaw_side      = right.m_y * g_ctx.m_cmd->m_side_move;
	const float roll_up       = up.m_z * g_ctx.m_cmd->m_up_move;

	const float x = cmd_forward.m_x * pitch_side + cmd_forward.m_y * yaw_side + cmd_forward.m_x * pitch_forward + cmd_forward.m_y * yaw_forward +
	                cmd_forward.m_z * roll_up;

	const float y =
		cmd_right.m_x * pitch_side + cmd_right.m_y * yaw_side + cmd_right.m_x * pitch_forward + cmd_right.m_y * yaw_forward + cmd_right.m_z * roll_up;

	const float cl_forwardspeed = g_convars[ HASH_BT( "cl_forwardspeed" ) ]->get_float( );
	const float cl_sidespeed    = g_convars[ HASH_BT( "cl_sidespeed" ) ]->get_float( );

	g_ctx.m_cmd->m_forward_move = std::clamp( x, -cl_forwardspeed, cl_forwardspeed );
	g_ctx.m_cmd->m_side_move    = std::clamp( y, -cl_sidespeed, cl_sidespeed );
}