#include "stdafx.h"
#include "panel_notifier.h"
#include "helpers.h"
#include "user_message.h"


/*static*/ panel_notifier_manager panel_notifier_manager::sm_instance;

namespace
{
	static service_factory_single_t<config_object_callback> g_config_object_callback;
	static playback_statistics_collector_factory_t<playback_stat_callback> g_stat_collector_callback;
	static play_callback_static_factory_t<my_play_callback > g_my_play_callback;
	static service_factory_single_t<my_playlist_callback> g_my_playlist_callback;
	static initquit_factory_t<metadb_changed_callback> g_metadb_changed_callback;
}

void panel_notifier_manager::send_msg_to_all(UINT p_msg, WPARAM p_wp, LPARAM p_lp)
{
	for (t_size i = 0; i < m_hwnds.get_count(); ++i)
	{
		HWND wnd = m_hwnds[i];

		SendMessage(wnd, p_msg, p_wp, p_lp);
	}
}

void panel_notifier_manager::post_msg_to_others_pointer(HWND p_wnd_except, UINT p_msg, pfc::refcounted_object_root * p_param)
{
	t_size count = m_hwnds.get_count();

	if (count < 2 || !p_param)
		return;

	for (t_size i = 0; i < count - 1; ++i)
		p_param->refcount_add_ref();

	for (t_size i = 0; i < count; ++i)
	{
		HWND wnd = m_hwnds[i];

		if (wnd != p_wnd_except)
		{
			PostMessage(wnd, p_msg, reinterpret_cast<WPARAM>(p_param), 0);
		}
	}
}

void panel_notifier_manager::post_msg_to_all(UINT p_msg, WPARAM p_wp, LPARAM p_lp)
{
	for (t_size i = 0; i < m_hwnds.get_count(); ++i)
	{
		HWND wnd = m_hwnds[i];

		PostMessage(wnd, p_msg, p_wp, p_lp);
	}
}

void panel_notifier_manager::post_msg_to_all_pointer(UINT p_msg, pfc::refcounted_object_root * p_param)
{
	t_size count = m_hwnds.get_count();

	if (count < 1 || !p_param)
		return;

	for (t_size i = 0; i < count; ++i)
		p_param->refcount_add_ref();

	for (t_size i = 0; i < count; ++i)
	{
		HWND wnd = m_hwnds[i];

		PostMessage(wnd, p_msg, reinterpret_cast<WPARAM>(p_param), 0);
	}
}

t_size config_object_callback::get_watched_object_count()
{
	return 3;
}

GUID config_object_callback::get_watched_object(t_size p_index)
{
	switch (p_index)
	{
	case 0:
		return standard_config_objects::bool_playlist_stop_after_current;

	case 1:
		return standard_config_objects::bool_cursor_follows_playback;

	case 2:
		return standard_config_objects::bool_playback_follows_cursor;
	}

	return pfc::guid_null;
}

void config_object_callback::on_watched_object_changed(const service_ptr_t<config_object> & p_object)
{
	GUID guid = p_object->get_guid();
	bool boolval = false;
	unsigned msg = 0;

	p_object->get_data_bool(boolval);

	if (guid == standard_config_objects::bool_playlist_stop_after_current)
		msg = CALLBACK_UWM_PLAYLIST_STOP_AFTER_CURRENT;
	else if (guid == standard_config_objects::bool_cursor_follows_playback)
		msg = CALLBACK_UWM_CURSOR_FOLLOW_PLAYBACK;
	else
		msg = CALLBACK_UWM_PLAYBACK_FOLLOW_CURSOR;

	panel_notifier_manager::instance().post_msg_to_all(msg, TO_VARIANT_BOOL(boolval), 0);
}

void playback_stat_callback::on_item_played(metadb_handle_ptr p_item)
{
	t_simple_callback_data<metadb_handle_ptr> * on_item_played_data 
		= new t_simple_callback_data<metadb_handle_ptr>(p_item);

	panel_notifier_manager::instance().post_msg_to_all_pointer(CALLBACK_UWM_ON_ITEM_PLAYED, 
		on_item_played_data);
}

void metadb_changed_callback::on_changed_sorted(metadb_handle_list_cref p_items_sorted, bool p_fromhook)
{
	t_on_changed_sorted_data * on_changed_sorted_data = new t_on_changed_sorted_data(p_items_sorted, p_fromhook);

	panel_notifier_manager::instance().post_msg_to_all_pointer(CALLBACK_UWM_ON_CHANGED_SORTED, 
		on_changed_sorted_data);
}

void metadb_changed_callback::on_init()
{
	static_api_ptr_t<metadb_io_v3> io;
	io->register_callback(this);
}

void metadb_changed_callback::on_quit()
{
	static_api_ptr_t<metadb_io_v3> io;
	io->unregister_callback(this);
}

void my_play_callback::on_playback_starting(play_control::t_track_command cmd, bool paused)
{
	panel_notifier_manager::instance().post_msg_to_all(CALLBACK_UWM_ON_PLAYBACK_STARTING, 
		(WPARAM)cmd, (LPARAM)paused);
}

void my_play_callback::on_playback_new_track(metadb_handle_ptr track)
{
	t_simple_callback_data<metadb_handle_ptr> * on_playback_new_track_data = new t_simple_callback_data<metadb_handle_ptr>(track);

	panel_notifier_manager::instance().post_msg_to_all_pointer(CALLBACK_UWM_ON_PLAYBACK_NEW_TRACK, 
		on_playback_new_track_data);
}

void my_play_callback::on_playback_stop(play_control::t_stop_reason reason)
{
	panel_notifier_manager::instance().post_msg_to_all(CALLBACK_UWM_ON_PLAYBACK_STOP, 
		(WPARAM)reason, 0);
}

void my_play_callback::on_playback_seek(double time)
{
	// sizeof(double) >= sizeof(WPARAM)
	t_simple_callback_data<double> * on_playback_seek_data = new t_simple_callback_data<double>(time);

	panel_notifier_manager::instance().post_msg_to_all_pointer(CALLBACK_UWM_ON_PLAYBACK_SEEK,
		on_playback_seek_data);
}

void my_play_callback::on_playback_pause(bool state)
{
	panel_notifier_manager::instance().post_msg_to_all(CALLBACK_UWM_ON_PLAYBACK_PAUSE,
		(WPARAM)state, 0);
}

void my_play_callback::on_playback_edited(metadb_handle_ptr track)
{
	panel_notifier_manager::instance().post_msg_to_all(CALLBACK_UWM_ON_PLAYBACK_EDITED, 0, 0);
}

void my_play_callback::on_playback_dynamic_info(const file_info& info)
{
	panel_notifier_manager::instance().post_msg_to_all(CALLBACK_UWM_ON_PLAYBACK_DYNAMIC_INFO, 0, 0);
}

void my_play_callback::on_playback_dynamic_info_track(const file_info& info)
{
	panel_notifier_manager::instance().post_msg_to_all(CALLBACK_UWM_ON_PLAYBACK_DYNAMIC_INFO_TRACK, 0, 0);
}

void my_play_callback::on_playback_time(double time)
{
	// sizeof(double) >= sizeof(WPARAM)
	t_simple_callback_data<double> * on_playback_time_data = new t_simple_callback_data<double>(time);

	panel_notifier_manager::instance().post_msg_to_all_pointer(CALLBACK_UWM_ON_PLAYBACK_TIME,
		on_playback_time_data);
}

void my_play_callback::on_volume_change(float newval)
{
	// though sizeof(float) == sizeof(int), cast of IEEE754 is dangerous, always.
	t_simple_callback_data<float> * on_volume_change_data = new t_simple_callback_data<float>(newval);

	panel_notifier_manager::instance().post_msg_to_all_pointer(CALLBACK_UWM_ON_VOLUME_CHANGE,
		on_volume_change_data);
}

void my_playlist_callback::on_item_focus_change(t_size p_from,t_size p_to)
{
	panel_notifier_manager::instance().post_msg_to_all(CALLBACK_UWM_ON_ITEM_FOCUS_CHANGE, 0, 0);
}

void my_playlist_callback::on_playback_order_changed(t_size p_new_index)
{
	panel_notifier_manager::instance().post_msg_to_all(CALLBACK_UWM_ON_PLAYBACK_ORDER_CHANGED, 
		(WPARAM)p_new_index, 0);
}
