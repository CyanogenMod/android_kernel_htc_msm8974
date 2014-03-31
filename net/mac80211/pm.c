#include <net/mac80211.h>
#include <net/rtnetlink.h>

#include "ieee80211_i.h"
#include "mesh.h"
#include "driver-ops.h"
#include "led.h"

static bool ieee80211_quiesce(struct ieee80211_sub_if_data *sdata)
{
	switch (sdata->vif.type) {
	case NL80211_IFTYPE_STATION:
		ieee80211_sta_quiesce(sdata);
		return true;
	case NL80211_IFTYPE_ADHOC:
		ieee80211_ibss_quiesce(sdata);
		return true;
	case NL80211_IFTYPE_MESH_POINT:
		ieee80211_mesh_quiesce(sdata);
		return true;
	case NL80211_IFTYPE_AP_VLAN:
	case NL80211_IFTYPE_MONITOR:
		
		return false;
	default:
		return true;
	}
}

int __ieee80211_suspend(struct ieee80211_hw *hw, struct cfg80211_wowlan *wowlan)
{
	struct ieee80211_local *local = hw_to_local(hw);
	struct ieee80211_sub_if_data *sdata;
	struct sta_info *sta;

	if (!local->open_count)
		goto suspend;

	ieee80211_scan_cancel(local);

	if (hw->flags & IEEE80211_HW_AMPDU_AGGREGATION) {
		mutex_lock(&local->sta_mtx);
		list_for_each_entry(sta, &local->sta_list, list) {
			set_sta_flag(sta, WLAN_STA_BLOCK_BA);
			ieee80211_sta_tear_down_BA_sessions(sta, true);
		}
		mutex_unlock(&local->sta_mtx);
	}

	ieee80211_stop_queues_by_reason(hw,
			IEEE80211_QUEUE_STOP_REASON_SUSPEND);

	
	synchronize_net();

	drv_flush(local, false);

	local->quiescing = true;
	
	mb();

	flush_workqueue(local->workqueue);

	
	del_timer_sync(&local->sta_cleanup);

	cancel_work_sync(&local->dynamic_ps_enable_work);
	del_timer_sync(&local->dynamic_ps_timer);

	local->wowlan = wowlan && local->open_count;
	if (local->wowlan) {
		int err = drv_suspend(local, wowlan);
		if (err < 0) {
			local->quiescing = false;
			return err;
		} else if (err > 0) {
			WARN_ON(err != 1);
			local->wowlan = false;
		} else {
			list_for_each_entry(sdata, &local->interfaces, list) {
				cancel_work_sync(&sdata->work);
				ieee80211_quiesce(sdata);
			}
			goto suspend;
		}
	}

	
	list_for_each_entry(sdata, &local->interfaces, list)
		ieee80211_disable_keys(sdata);

	
	mutex_lock(&local->sta_mtx);
	list_for_each_entry(sta, &local->sta_list, list) {
		if (sta->uploaded) {
			enum ieee80211_sta_state state;

			state = sta->sta_state;
			for (; state > IEEE80211_STA_NOTEXIST; state--)
				WARN_ON(drv_sta_state(local, sta->sdata, sta,
						      state, state - 1));
		}

		mesh_plink_quiesce(sta);
	}
	mutex_unlock(&local->sta_mtx);

	
	list_for_each_entry(sdata, &local->interfaces, list) {
		cancel_work_sync(&sdata->work);

		if (!ieee80211_quiesce(sdata))
			continue;

		if (!ieee80211_sdata_running(sdata))
			continue;

		
		ieee80211_bss_info_change_notify(sdata,
			BSS_CHANGED_BEACON_ENABLED);

		drv_remove_interface(local, sdata);
	}

	
	if (local->open_count)
		ieee80211_stop_device(local);

 suspend:
	local->suspended = true;
	
	barrier();
	local->quiescing = false;

	return 0;
}

