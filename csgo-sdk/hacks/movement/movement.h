#pragma once

class c_user_cmd;
class c_angle;

enum class edgebug_method_t : int {
	none = 0,
	standing,
	ducking
};

namespace n_movement
{
	struct impl_t {
		struct edgebug_data_t {
			bool m_will_edgebug = false;
			bool m_will_fail    = false;
			bool m_strafing     = false;

			int m_ticks_to_stop = 0;
			int m_last_tick     = 0;

			int m_saved_mousedx = 0;

			float m_starting_yaw = 0.f;
			float m_yaw_step     = 0.f;

			float m_forward_move = 0.f;
			float m_side_move    = 0.f;

			edgebug_method_t m_method = edgebug_method_t::none;

			__forceinline void reset( )
			{
				m_will_edgebug = false;
				m_will_fail    = false;
				m_strafing     = false;

				m_ticks_to_stop = 0;
				m_last_tick     = 0;
				m_saved_mousedx = 0;

				m_starting_yaw = 0.f;
				m_yaw_step     = 0.f;

				m_forward_move = 0.f;
				m_side_move    = 0.f;

				m_method = edgebug_method_t::none;
			}
		} m_edgebug_data;

		struct jumpbug_data_t {
			bool m_will_should{ };
		} m_jumpbug_data;

		struct pixelsurf_data_t {
			bool m_did_ducking{ };
			bool m_detected{ };
		} m_pixelsurf_data;

		struct autoduck_data_t {
			bool m_did_land_ducking  = false;
			bool m_did_land_standing = false;

			float m_ducking_vert  = 0.f;
			float m_standing_vert = 0.f;

			void reset( );
		} m_autoduck_data;

		void on_create_move_pre( );
		void on_create_move_post( );

		void on_frame_stage_notify( int stage );
private:
		void bunny_hop( );

		void edge_jump( );

		void edge_bug( );

		void long_jump( );

		void mini_jump( );

		void jump_bug( );

		void auto_duck( );

		void pixel_surf( );

		void jump_bug_simulation( );

		void pixel_surf_fix( );

		void detect_edgebug( c_user_cmd* cmd );

		void auto_align( );

		void movement_fix( const c_angle& old_view_point );
		
		void infinity_duck( );
	};
} 

inline n_movement::impl_t g_movement{ };
