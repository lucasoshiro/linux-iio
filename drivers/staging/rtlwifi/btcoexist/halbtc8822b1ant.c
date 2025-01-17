/******************************************************************************
 *
 * Copyright(c) 2016  Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/
/* ************************************************************
 * Description:
 *
 * This file is for RTL8822B Co-exist mechanism
 *
 * History
 * 2012/11/15 Cosa first check in.
 *
 * *************************************************************/

/* ************************************************************
 * include files
 * *************************************************************/
/*only for rf4ce*/
#include "halbt_precomp.h"

/* ************************************************************
 * Global variables, these are static variables
 * *************************************************************/
static struct coex_dm_8822b_1ant glcoex_dm_8822b_1ant;
static struct coex_dm_8822b_1ant *coex_dm = &glcoex_dm_8822b_1ant;
static struct coex_sta_8822b_1ant glcoex_sta_8822b_1ant;
static struct coex_sta_8822b_1ant *coex_sta = &glcoex_sta_8822b_1ant;
static struct psdscan_sta_8822b_1ant gl_psd_scan_8822b_1ant;
static struct psdscan_sta_8822b_1ant *psd_scan = &gl_psd_scan_8822b_1ant;
static struct rfe_type_8822b_1ant gl_rfe_type_8822b_1ant;
static struct rfe_type_8822b_1ant *rfe_type = &gl_rfe_type_8822b_1ant;

static const char *const glbt_info_src_8822b_1ant[] = {
	"BT Info[wifi fw]", "BT Info[bt rsp]", "BT Info[bt auto report]",
};

static u32 glcoex_ver_date_8822b_1ant = 20170327;
static u32 glcoex_ver_8822b_1ant = 0x44;
static u32 glcoex_ver_btdesired_8822b_1ant = 0x42;

/* ************************************************************
 * local function proto type if needed
 * ************************************************************
 * ************************************************************
 * local function start with halbtc8822b1ant_
 * *************************************************************/

static u8 halbtc8822b1ant_wifi_rssi_state(struct btc_coexist *btcoexist,
					  u8 index, u8 level_num,
					  u8 rssi_thresh, u8 rssi_thresh1)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	s32 wifi_rssi = 0;
	u8 wifi_rssi_state = coex_sta->pre_wifi_rssi_state[index];

	btcoexist->btc_get(btcoexist, BTC_GET_S4_WIFI_RSSI, &wifi_rssi);

	if (level_num == 2) {
		if ((coex_sta->pre_wifi_rssi_state[index] ==
		     BTC_RSSI_STATE_LOW) ||
		    (coex_sta->pre_wifi_rssi_state[index] ==
		     BTC_RSSI_STATE_STAY_LOW)) {
			if (wifi_rssi >=
			    (rssi_thresh + BTC_RSSI_COEX_THRESH_TOL_8822B_1ANT))
				wifi_rssi_state = BTC_RSSI_STATE_HIGH;
			else
				wifi_rssi_state = BTC_RSSI_STATE_STAY_LOW;
		} else {
			if (wifi_rssi < rssi_thresh)
				wifi_rssi_state = BTC_RSSI_STATE_LOW;
			else
				wifi_rssi_state = BTC_RSSI_STATE_STAY_HIGH;
		}
	} else if (level_num == 3) {
		if (rssi_thresh > rssi_thresh1) {
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "[BTCoex], wifi RSSI thresh error!!\n");
			return coex_sta->pre_wifi_rssi_state[index];
		}

		if ((coex_sta->pre_wifi_rssi_state[index] ==
		     BTC_RSSI_STATE_LOW) ||
		    (coex_sta->pre_wifi_rssi_state[index] ==
		     BTC_RSSI_STATE_STAY_LOW)) {
			if (wifi_rssi >=
			    (rssi_thresh + BTC_RSSI_COEX_THRESH_TOL_8822B_1ANT))
				wifi_rssi_state = BTC_RSSI_STATE_MEDIUM;
			else
				wifi_rssi_state = BTC_RSSI_STATE_STAY_LOW;
		} else if ((coex_sta->pre_wifi_rssi_state[index] ==
			    BTC_RSSI_STATE_MEDIUM) ||
			   (coex_sta->pre_wifi_rssi_state[index] ==
			    BTC_RSSI_STATE_STAY_MEDIUM)) {
			if (wifi_rssi >= (rssi_thresh1 +
					  BTC_RSSI_COEX_THRESH_TOL_8822B_1ANT))
				wifi_rssi_state = BTC_RSSI_STATE_HIGH;
			else if (wifi_rssi < rssi_thresh)
				wifi_rssi_state = BTC_RSSI_STATE_LOW;
			else
				wifi_rssi_state = BTC_RSSI_STATE_STAY_MEDIUM;
		} else {
			if (wifi_rssi < rssi_thresh1)
				wifi_rssi_state = BTC_RSSI_STATE_MEDIUM;
			else
				wifi_rssi_state = BTC_RSSI_STATE_STAY_HIGH;
		}
	}

	coex_sta->pre_wifi_rssi_state[index] = wifi_rssi_state;

	return wifi_rssi_state;
}

static void halbtc8822b1ant_update_ra_mask(struct btc_coexist *btcoexist,
					   bool force_exec, u32 dis_rate_mask)
{
	coex_dm->cur_ra_mask = dis_rate_mask;

	if (force_exec || (coex_dm->pre_ra_mask != coex_dm->cur_ra_mask))
		btcoexist->btc_set(btcoexist, BTC_SET_ACT_UPDATE_RAMASK,
				   &coex_dm->cur_ra_mask);
	coex_dm->pre_ra_mask = coex_dm->cur_ra_mask;
}

static void
halbtc8822b1ant_auto_rate_fallback_retry(struct btc_coexist *btcoexist,
					 bool force_exec, u8 type)
{
	bool wifi_under_b_mode = false;

	coex_dm->cur_arfr_type = type;

	if (force_exec || (coex_dm->pre_arfr_type != coex_dm->cur_arfr_type)) {
		switch (coex_dm->cur_arfr_type) {
		case 0: /* normal mode */
			btcoexist->btc_write_4byte(btcoexist, 0x430,
						   coex_dm->backup_arfr_cnt1);
			btcoexist->btc_write_4byte(btcoexist, 0x434,
						   coex_dm->backup_arfr_cnt2);
			break;
		case 1:
			btcoexist->btc_get(btcoexist,
					   BTC_GET_BL_WIFI_UNDER_B_MODE,
					   &wifi_under_b_mode);
			if (wifi_under_b_mode) {
				btcoexist->btc_write_4byte(btcoexist, 0x430,
							   0x0);
				btcoexist->btc_write_4byte(btcoexist, 0x434,
							   0x01010101);
			} else {
				btcoexist->btc_write_4byte(btcoexist, 0x430,
							   0x0);
				btcoexist->btc_write_4byte(btcoexist, 0x434,
							   0x04030201);
			}
			break;
		default:
			break;
		}
	}

	coex_dm->pre_arfr_type = coex_dm->cur_arfr_type;
}

static void halbtc8822b1ant_retry_limit(struct btc_coexist *btcoexist,
					bool force_exec, u8 type)
{
	coex_dm->cur_retry_limit_type = type;

	if (force_exec ||
	    (coex_dm->pre_retry_limit_type != coex_dm->cur_retry_limit_type)) {
		switch (coex_dm->cur_retry_limit_type) {
		case 0: /* normal mode */
			btcoexist->btc_write_2byte(btcoexist, 0x42a,
						   coex_dm->backup_retry_limit);
			break;
		case 1: /* retry limit=8 */
			btcoexist->btc_write_2byte(btcoexist, 0x42a, 0x0808);
			break;
		default:
			break;
		}
	}

	coex_dm->pre_retry_limit_type = coex_dm->cur_retry_limit_type;
}

static void halbtc8822b1ant_ampdu_max_time(struct btc_coexist *btcoexist,
					   bool force_exec, u8 type)
{
	coex_dm->cur_ampdu_time_type = type;

	if (force_exec ||
	    (coex_dm->pre_ampdu_time_type != coex_dm->cur_ampdu_time_type)) {
		switch (coex_dm->cur_ampdu_time_type) {
		case 0: /* normal mode */
			btcoexist->btc_write_1byte(
				btcoexist, 0x456,
				coex_dm->backup_ampdu_max_time);
			break;
		case 1: /* AMPDU timw = 0x38 * 32us */
			btcoexist->btc_write_1byte(btcoexist, 0x456, 0x38);
			break;
		default:
			break;
		}
	}

	coex_dm->pre_ampdu_time_type = coex_dm->cur_ampdu_time_type;
}

static void halbtc8822b1ant_limited_tx(struct btc_coexist *btcoexist,
				       bool force_exec, u8 ra_mask_type,
				       u8 arfr_type, u8 retry_limit_type,
				       u8 ampdu_time_type)
{
	switch (ra_mask_type) {
	case 0: /* normal mode */
		halbtc8822b1ant_update_ra_mask(btcoexist, force_exec, 0x0);
		break;
	case 1: /* disable cck 1/2 */
		halbtc8822b1ant_update_ra_mask(btcoexist, force_exec,
					       0x00000003);
		break;
	case 2: /* disable cck 1/2/5.5, ofdm 6/9/12/18/24, mcs 0/1/2/3/4 */
		halbtc8822b1ant_update_ra_mask(btcoexist, force_exec,
					       0x0001f1f7);
		break;
	default:
		break;
	}

	halbtc8822b1ant_auto_rate_fallback_retry(btcoexist, force_exec,
						 arfr_type);
	halbtc8822b1ant_retry_limit(btcoexist, force_exec, retry_limit_type);
	halbtc8822b1ant_ampdu_max_time(btcoexist, force_exec, ampdu_time_type);
}

/*
 * rx agg size setting :
 * 1:      true / don't care / don't care
 * max: false / false / don't care
 * 7:     false / true / 7
 */

static void halbtc8822b1ant_limited_rx(struct btc_coexist *btcoexist,
				       bool force_exec, bool rej_ap_agg_pkt,
				       bool bt_ctrl_agg_buf_size,
				       u8 agg_buf_size)
{
	bool reject_rx_agg = rej_ap_agg_pkt;
	bool bt_ctrl_rx_agg_size = bt_ctrl_agg_buf_size;
	u8 rx_agg_size = agg_buf_size;

	/* ============================================ */
	/*	Rx Aggregation related setting */
	/* ============================================ */
	btcoexist->btc_set(btcoexist, BTC_SET_BL_TO_REJ_AP_AGG_PKT,
			   &reject_rx_agg);
	/* decide BT control aggregation buf size or not */
	btcoexist->btc_set(btcoexist, BTC_SET_BL_BT_CTRL_AGG_SIZE,
			   &bt_ctrl_rx_agg_size);
	/* aggregation buf size, only work when BT control Rx aggregation size*/
	btcoexist->btc_set(btcoexist, BTC_SET_U1_AGG_BUF_SIZE, &rx_agg_size);
	/* real update aggregation setting */
	btcoexist->btc_set(btcoexist, BTC_SET_ACT_AGGREGATE_CTRL, NULL);
}

static void halbtc8822b1ant_query_bt_info(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	u8 h2c_parameter[1] = {0};

	if (coex_sta->bt_disabled) {
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], No query BT info because BT is disabled!\n");
		return;
	}

	h2c_parameter[0] |= BIT(0); /* trigger */

	btcoexist->btc_fill_h2c(btcoexist, 0x61, 1, h2c_parameter);

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex], WL query BT info!!\n");
}

static void halbtc8822b1ant_monitor_bt_ctr(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	u32 reg_hp_txrx, reg_lp_txrx, u32tmp;
	u32 reg_hp_tx = 0, reg_hp_rx = 0, reg_lp_tx = 0, reg_lp_rx = 0;
	static u8 num_of_bt_counter_chk, cnt_slave, cnt_autoslot_hang;
	struct btc_bt_link_info *bt_link_info = &btcoexist->bt_link_info;

	reg_hp_txrx = 0x770;
	reg_lp_txrx = 0x774;

	u32tmp = btcoexist->btc_read_4byte(btcoexist, reg_hp_txrx);
	reg_hp_tx = u32tmp & MASKLWORD;
	reg_hp_rx = (u32tmp & MASKHWORD) >> 16;

	u32tmp = btcoexist->btc_read_4byte(btcoexist, reg_lp_txrx);
	reg_lp_tx = u32tmp & MASKLWORD;
	reg_lp_rx = (u32tmp & MASKHWORD) >> 16;

	coex_sta->high_priority_tx = reg_hp_tx;
	coex_sta->high_priority_rx = reg_hp_rx;
	coex_sta->low_priority_tx = reg_lp_tx;
	coex_sta->low_priority_rx = reg_lp_rx;

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex], Hi-Pri Rx/Tx: %d/%d, Lo-Pri Rx/Tx: %d/%d\n",
		 reg_hp_rx, reg_hp_tx, reg_lp_rx, reg_lp_tx);

	/* reset counter */
	btcoexist->btc_write_1byte(btcoexist, 0x76e, 0xc);

	if ((coex_sta->low_priority_tx > 1150) &&
	    (!coex_sta->c2h_bt_inquiry_page))
		coex_sta->pop_event_cnt++;

	if ((coex_sta->low_priority_rx >= 1150) &&
	    (coex_sta->low_priority_rx >= coex_sta->low_priority_tx) &&
	    (!coex_sta->under_ips) && (!coex_sta->c2h_bt_inquiry_page) &&
	    (coex_sta->bt_link_exist)) {
		if (cnt_slave >= 3) {
			bt_link_info->slave_role = true;
			cnt_slave = 3;
		} else {
			cnt_slave++;
		}
	} else {
		if (cnt_slave == 0) {
			bt_link_info->slave_role = false;
			cnt_slave = 0;
		} else {
			cnt_slave--;
		}
	}

	if (coex_sta->is_tdma_btautoslot) {
		if ((coex_sta->low_priority_tx >= 1300) &&
		    (coex_sta->low_priority_rx <= 150)) {
			if (cnt_autoslot_hang >= 2) {
				coex_sta->is_tdma_btautoslot_hang = true;
				cnt_autoslot_hang = 2;
			} else {
				cnt_autoslot_hang++;
			}
		} else {
			if (cnt_autoslot_hang == 0) {
				coex_sta->is_tdma_btautoslot_hang = false;
				cnt_autoslot_hang = 0;
			} else {
				cnt_autoslot_hang--;
			}
		}
	}

	if (bt_link_info->hid_only) {
		if (coex_sta->low_priority_rx > 50)
			coex_sta->is_hid_low_pri_tx_overhead = true;
		else
			coex_sta->is_hid_low_pri_tx_overhead = false;
	}

	if ((coex_sta->high_priority_tx == 0) &&
	    (coex_sta->high_priority_rx == 0) &&
	    (coex_sta->low_priority_tx == 0) &&
	    (coex_sta->low_priority_rx == 0)) {
		num_of_bt_counter_chk++;

		if (num_of_bt_counter_chk >= 3) {
			halbtc8822b1ant_query_bt_info(btcoexist);
			num_of_bt_counter_chk = 0;
		}
	}
}

static void halbtc8822b1ant_monitor_wifi_ctr(struct btc_coexist *btcoexist)
{
	s32 wifi_rssi = 0;
	bool wifi_busy = false, wifi_under_b_mode = false, wifi_scan = false;
	static u8 cck_lock_counter, wl_noisy_count0, wl_noisy_count1 = 3,
						     wl_noisy_count2;
	u32 total_cnt, cck_cnt;

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_BUSY, &wifi_busy);
	btcoexist->btc_get(btcoexist, BTC_GET_S4_WIFI_RSSI, &wifi_rssi);
	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_UNDER_B_MODE,
			   &wifi_under_b_mode);

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_SCAN, &wifi_scan);

	coex_sta->crc_ok_cck = btcoexist->btc_phydm_query_phy_counter(
		btcoexist, "PHYDM_INFO_CRC32_OK_CCK");
	coex_sta->crc_ok_11g = btcoexist->btc_phydm_query_phy_counter(
		btcoexist, "PHYDM_INFO_CRC32_OK_LEGACY");
	coex_sta->crc_ok_11n = btcoexist->btc_phydm_query_phy_counter(
		btcoexist, "PHYDM_INFO_CRC32_OK_HT");
	coex_sta->crc_ok_11n_vht = btcoexist->btc_phydm_query_phy_counter(
		btcoexist, "PHYDM_INFO_CRC32_OK_VHT");

	coex_sta->crc_err_cck = btcoexist->btc_phydm_query_phy_counter(
		btcoexist, "PHYDM_INFO_CRC32_ERROR_CCK");
	coex_sta->crc_err_11g = btcoexist->btc_phydm_query_phy_counter(
		btcoexist, "PHYDM_INFO_CRC32_ERROR_LEGACY");
	coex_sta->crc_err_11n = btcoexist->btc_phydm_query_phy_counter(
		btcoexist, "PHYDM_INFO_CRC32_ERROR_HT");
	coex_sta->crc_err_11n_vht = btcoexist->btc_phydm_query_phy_counter(
		btcoexist, "PHYDM_INFO_CRC32_ERROR_VHT");

	cck_cnt = coex_sta->crc_ok_cck + coex_sta->crc_err_cck;

	if (cck_cnt > 250) {
		if (wl_noisy_count2 < 3)
			wl_noisy_count2++;

		if (wl_noisy_count2 == 3) {
			wl_noisy_count0 = 0;
			wl_noisy_count1 = 0;
		}

	} else if (cck_cnt < 50) {
		if (wl_noisy_count0 < 3)
			wl_noisy_count0++;

		if (wl_noisy_count0 == 3) {
			wl_noisy_count1 = 0;
			wl_noisy_count2 = 0;
		}

	} else {
		if (wl_noisy_count1 < 3)
			wl_noisy_count1++;

		if (wl_noisy_count1 == 3) {
			wl_noisy_count0 = 0;
			wl_noisy_count2 = 0;
		}
	}

	if (wl_noisy_count2 == 3)
		coex_sta->wl_noisy_level = 2;
	else if (wl_noisy_count1 == 3)
		coex_sta->wl_noisy_level = 1;
	else
		coex_sta->wl_noisy_level = 0;

	if ((wifi_busy) && (wifi_rssi >= 30) && (!wifi_under_b_mode)) {
		total_cnt = coex_sta->crc_ok_cck + coex_sta->crc_ok_11g +
			    coex_sta->crc_ok_11n + coex_sta->crc_ok_11n_vht;

		if ((coex_dm->bt_status == BT_8822B_1ANT_BT_STATUS_ACL_BUSY) ||
		    (coex_dm->bt_status ==
		     BT_8822B_1ANT_BT_STATUS_ACL_SCO_BUSY) ||
		    (coex_dm->bt_status == BT_8822B_1ANT_BT_STATUS_SCO_BUSY)) {
			if (coex_sta->crc_ok_cck >
			    (total_cnt - coex_sta->crc_ok_cck)) {
				if (cck_lock_counter < 3)
					cck_lock_counter++;
			} else {
				if (cck_lock_counter > 0)
					cck_lock_counter--;
			}

		} else {
			if (cck_lock_counter > 0)
				cck_lock_counter--;
		}
	} else {
		if (cck_lock_counter > 0)
			cck_lock_counter--;
	}

	if (!coex_sta->pre_ccklock) {
		if (cck_lock_counter >= 3)
			coex_sta->cck_lock = true;
		else
			coex_sta->cck_lock = false;
	} else {
		if (cck_lock_counter == 0)
			coex_sta->cck_lock = false;
		else
			coex_sta->cck_lock = true;
	}

	if (coex_sta->cck_lock)
		coex_sta->cck_ever_lock = true;

	coex_sta->pre_ccklock = coex_sta->cck_lock;
}

static bool
halbtc8822b1ant_is_wifi_status_changed(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	static bool pre_wifi_busy, pre_under_4way, pre_bt_hs_on,
		pre_rf4ce_enabled, pre_bt_off, pre_bt_slave,
		pre_hid_low_pri_tx_overhead, pre_wifi_under_lps,
		pre_bt_setup_link;
	static u8 pre_hid_busy_num, pre_wl_noisy_level;
	bool wifi_busy = false, under_4way = false, bt_hs_on = false,
	     rf4ce_enabled = false;
	bool wifi_connected = false;
	struct btc_bt_link_info *bt_link_info = &btcoexist->bt_link_info;

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_CONNECTED,
			   &wifi_connected);
	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_BUSY, &wifi_busy);
	btcoexist->btc_get(btcoexist, BTC_GET_BL_HS_OPERATION, &bt_hs_on);
	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_4_WAY_PROGRESS,
			   &under_4way);

	if (coex_sta->bt_disabled != pre_bt_off) {
		pre_bt_off = coex_sta->bt_disabled;

		if (coex_sta->bt_disabled)
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "[BTCoex], BT is disabled !!\n");
		else
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "[BTCoex], BT is enabled !!\n");

		coex_sta->bt_coex_supported_feature = 0;
		coex_sta->bt_coex_supported_version = 0;
		return true;
	}
	btcoexist->btc_get(btcoexist, BTC_GET_BL_RF4CE_CONNECTED,
			   &rf4ce_enabled);

	if (rf4ce_enabled != pre_rf4ce_enabled) {
		pre_rf4ce_enabled = rf4ce_enabled;

		if (rf4ce_enabled)
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "[BTCoex], rf4ce is enabled !!\n");
		else
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "[BTCoex], rf4ce is disabled !!\n");

		return true;
	}

	if (wifi_connected) {
		if (wifi_busy != pre_wifi_busy) {
			pre_wifi_busy = wifi_busy;
			return true;
		}
		if (under_4way != pre_under_4way) {
			pre_under_4way = under_4way;
			return true;
		}
		if (bt_hs_on != pre_bt_hs_on) {
			pre_bt_hs_on = bt_hs_on;
			return true;
		}
		if (coex_sta->wl_noisy_level != pre_wl_noisy_level) {
			pre_wl_noisy_level = coex_sta->wl_noisy_level;
			return true;
		}
		if (coex_sta->under_lps != pre_wifi_under_lps) {
			pre_wifi_under_lps = coex_sta->under_lps;
			if (coex_sta->under_lps)
				return true;
		}
	}

	if (!coex_sta->bt_disabled) {
		if (coex_sta->hid_busy_num != pre_hid_busy_num) {
			pre_hid_busy_num = coex_sta->hid_busy_num;
			return true;
		}

		if (bt_link_info->slave_role != pre_bt_slave) {
			pre_bt_slave = bt_link_info->slave_role;
			return true;
		}

		if (pre_hid_low_pri_tx_overhead !=
		    coex_sta->is_hid_low_pri_tx_overhead) {
			pre_hid_low_pri_tx_overhead =
				coex_sta->is_hid_low_pri_tx_overhead;
			return true;
		}

		if (pre_bt_setup_link != coex_sta->is_setup_link) {
			pre_bt_setup_link = coex_sta->is_setup_link;
			return true;
		}
	}

	return false;
}

static void halbtc8822b1ant_update_bt_link_info(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	struct btc_bt_link_info *bt_link_info = &btcoexist->bt_link_info;
	bool bt_hs_on = false;
	bool bt_busy = false;

	coex_sta->num_of_profile = 0;

	/* set link exist status */
	if (!(coex_sta->bt_info & BT_INFO_8822B_1ANT_B_CONNECTION)) {
		coex_sta->bt_link_exist = false;
		coex_sta->pan_exist = false;
		coex_sta->a2dp_exist = false;
		coex_sta->hid_exist = false;
		coex_sta->sco_exist = false;
	} else { /* connection exists */
		coex_sta->bt_link_exist = true;
		if (coex_sta->bt_info & BT_INFO_8822B_1ANT_B_FTP) {
			coex_sta->pan_exist = true;
			coex_sta->num_of_profile++;
		} else {
			coex_sta->pan_exist = false;
		}

		if (coex_sta->bt_info & BT_INFO_8822B_1ANT_B_A2DP) {
			coex_sta->a2dp_exist = true;
			coex_sta->num_of_profile++;
		} else {
			coex_sta->a2dp_exist = false;
		}

		if (coex_sta->bt_info & BT_INFO_8822B_1ANT_B_HID) {
			coex_sta->hid_exist = true;
			coex_sta->num_of_profile++;
		} else {
			coex_sta->hid_exist = false;
		}

		if (coex_sta->bt_info & BT_INFO_8822B_1ANT_B_SCO_ESCO) {
			coex_sta->sco_exist = true;
			coex_sta->num_of_profile++;
		} else {
			coex_sta->sco_exist = false;
		}
	}

	btcoexist->btc_get(btcoexist, BTC_GET_BL_HS_OPERATION, &bt_hs_on);

	bt_link_info->bt_link_exist = coex_sta->bt_link_exist;
	bt_link_info->sco_exist = coex_sta->sco_exist;
	bt_link_info->a2dp_exist = coex_sta->a2dp_exist;
	bt_link_info->pan_exist = coex_sta->pan_exist;
	bt_link_info->hid_exist = coex_sta->hid_exist;
	bt_link_info->acl_busy = coex_sta->acl_busy;

	/* work around for HS mode. */
	if (bt_hs_on) {
		bt_link_info->pan_exist = true;
		bt_link_info->bt_link_exist = true;
	}

	/* check if Sco only */
	if (bt_link_info->sco_exist && !bt_link_info->a2dp_exist &&
	    !bt_link_info->pan_exist && !bt_link_info->hid_exist)
		bt_link_info->sco_only = true;
	else
		bt_link_info->sco_only = false;

	/* check if A2dp only */
	if (!bt_link_info->sco_exist && bt_link_info->a2dp_exist &&
	    !bt_link_info->pan_exist && !bt_link_info->hid_exist)
		bt_link_info->a2dp_only = true;
	else
		bt_link_info->a2dp_only = false;

	/* check if Pan only */
	if (!bt_link_info->sco_exist && !bt_link_info->a2dp_exist &&
	    bt_link_info->pan_exist && !bt_link_info->hid_exist)
		bt_link_info->pan_only = true;
	else
		bt_link_info->pan_only = false;

	/* check if Hid only */
	if (!bt_link_info->sco_exist && !bt_link_info->a2dp_exist &&
	    !bt_link_info->pan_exist && bt_link_info->hid_exist)
		bt_link_info->hid_only = true;
	else
		bt_link_info->hid_only = false;

	if (coex_sta->bt_info & BT_INFO_8822B_1ANT_B_INQ_PAGE) {
		coex_dm->bt_status = BT_8822B_1ANT_BT_STATUS_INQ_PAGE;
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], BtInfoNotify(), BT Inq/page!!!\n");
	} else if (!(coex_sta->bt_info & BT_INFO_8822B_1ANT_B_CONNECTION)) {
		coex_dm->bt_status = BT_8822B_1ANT_BT_STATUS_NON_CONNECTED_IDLE;
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], BtInfoNotify(), BT Non-Connected idle!!!\n");
	} else if (coex_sta->bt_info == BT_INFO_8822B_1ANT_B_CONNECTION) {
		/* connection exists but no busy */
		coex_dm->bt_status = BT_8822B_1ANT_BT_STATUS_CONNECTED_IDLE;
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], BtInfoNotify(), BT Connected-idle!!!\n");
	} else if (((coex_sta->bt_info & BT_INFO_8822B_1ANT_B_SCO_ESCO) ||
		    (coex_sta->bt_info & BT_INFO_8822B_1ANT_B_SCO_BUSY)) &&
		   (coex_sta->bt_info & BT_INFO_8822B_1ANT_B_ACL_BUSY)) {
		coex_dm->bt_status = BT_8822B_1ANT_BT_STATUS_ACL_SCO_BUSY;
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], BtInfoNotify(), BT ACL SCO busy!!!\n");
	} else if ((coex_sta->bt_info & BT_INFO_8822B_1ANT_B_SCO_ESCO) ||
		   (coex_sta->bt_info & BT_INFO_8822B_1ANT_B_SCO_BUSY)) {
		coex_dm->bt_status = BT_8822B_1ANT_BT_STATUS_SCO_BUSY;
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], BtInfoNotify(), BT SCO busy!!!\n");
	} else if (coex_sta->bt_info & BT_INFO_8822B_1ANT_B_ACL_BUSY) {
		coex_dm->bt_status = BT_8822B_1ANT_BT_STATUS_ACL_BUSY;
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], BtInfoNotify(), BT ACL busy!!!\n");
	} else {
		coex_dm->bt_status = BT_8822B_1ANT_BT_STATUS_MAX;
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], BtInfoNotify(), BT Non-Defined state!!!\n");
	}

	if ((coex_dm->bt_status == BT_8822B_1ANT_BT_STATUS_ACL_BUSY) ||
	    (coex_dm->bt_status == BT_8822B_1ANT_BT_STATUS_SCO_BUSY) ||
	    (coex_dm->bt_status == BT_8822B_1ANT_BT_STATUS_ACL_SCO_BUSY))
		bt_busy = true;
	else
		bt_busy = false;

	btcoexist->btc_set(btcoexist, BTC_SET_BL_BT_TRAFFIC_BUSY, &bt_busy);
}

static void halbtc8822b1ant_update_wifi_ch_info(struct btc_coexist *btcoexist,
						u8 type)
{
	u8 h2c_parameter[3] = {0};
	u32 wifi_bw;
	u8 wifi_central_chnl;

	/* only 2.4G we need to inform bt the chnl mask */
	btcoexist->btc_get(btcoexist, BTC_GET_U1_WIFI_CENTRAL_CHNL,
			   &wifi_central_chnl);
	if ((type == BTC_MEDIA_CONNECT) && (wifi_central_chnl <= 14)) {
		/* enable BT AFH skip WL channel for 8822b
		 * because BT Rx LO interference
		 */
		h2c_parameter[0] = 0x1;
		h2c_parameter[1] = wifi_central_chnl;

		btcoexist->btc_get(btcoexist, BTC_GET_U4_WIFI_BW, &wifi_bw);

		if (wifi_bw == BTC_WIFI_BW_HT40)
			h2c_parameter[2] = 0x30;
		else
			h2c_parameter[2] = 0x20;
	}

	coex_dm->wifi_chnl_info[0] = h2c_parameter[0];
	coex_dm->wifi_chnl_info[1] = h2c_parameter[1];
	coex_dm->wifi_chnl_info[2] = h2c_parameter[2];

	btcoexist->btc_fill_h2c(btcoexist, 0x66, 3, h2c_parameter);
}

static u8 halbtc8822b1ant_action_algorithm(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	struct btc_bt_link_info *bt_link_info = &btcoexist->bt_link_info;
	bool bt_hs_on = false;
	u8 algorithm = BT_8822B_1ANT_COEX_ALGO_UNDEFINED;
	u8 num_of_diff_profile = 0;

	btcoexist->btc_get(btcoexist, BTC_GET_BL_HS_OPERATION, &bt_hs_on);

	if (!bt_link_info->bt_link_exist) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], No BT link exists!!!\n");
		return algorithm;
	}

	if (bt_link_info->sco_exist)
		num_of_diff_profile++;
	if (bt_link_info->hid_exist)
		num_of_diff_profile++;
	if (bt_link_info->pan_exist)
		num_of_diff_profile++;
	if (bt_link_info->a2dp_exist)
		num_of_diff_profile++;

	if (num_of_diff_profile == 1) {
		if (bt_link_info->sco_exist) {
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "[BTCoex], BT Profile = SCO only\n");
			algorithm = BT_8822B_1ANT_COEX_ALGO_SCO;
		} else {
			if (bt_link_info->hid_exist) {
				RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
					 "[BTCoex], BT Profile = HID only\n");
				algorithm = BT_8822B_1ANT_COEX_ALGO_HID;
			} else if (bt_link_info->a2dp_exist) {
				RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
					 "[BTCoex], BT Profile = A2DP only\n");
				algorithm = BT_8822B_1ANT_COEX_ALGO_A2DP;
			} else if (bt_link_info->pan_exist) {
				if (bt_hs_on) {
					RT_TRACE(
						rtlpriv, COMP_BT_COEXIST,
						DBG_LOUD,
						"[BTCoex], BT Profile = PAN(HS) only\n");
					algorithm =
						BT_8822B_1ANT_COEX_ALGO_PANHS;
				} else {
					RT_TRACE(
						rtlpriv, COMP_BT_COEXIST,
						DBG_LOUD,
						"[BTCoex], BT Profile = PAN(EDR) only\n");
					algorithm =
						BT_8822B_1ANT_COEX_ALGO_PANEDR;
				}
			}
		}
	} else if (num_of_diff_profile == 2) {
		if (bt_link_info->sco_exist) {
			if (bt_link_info->hid_exist) {
				RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
					 "[BTCoex], BT Profile = SCO + HID\n");
				algorithm = BT_8822B_1ANT_COEX_ALGO_HID;
			} else if (bt_link_info->a2dp_exist) {
				RT_TRACE(
					rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
					"[BTCoex], BT Profile = SCO + A2DP ==> SCO\n");
				algorithm = BT_8822B_1ANT_COEX_ALGO_SCO;
			} else if (bt_link_info->pan_exist) {
				if (bt_hs_on) {
					RT_TRACE(
						rtlpriv, COMP_BT_COEXIST,
						DBG_LOUD,
						"[BTCoex], BT Profile = SCO + PAN(HS)\n");
					algorithm = BT_8822B_1ANT_COEX_ALGO_SCO;
				} else {
					RT_TRACE(
						rtlpriv, COMP_BT_COEXIST,
						DBG_LOUD,
						"[BTCoex], BT Profile = SCO + PAN(EDR)\n");
					algorithm =
					    BT_8822B_1ANT_COEX_ALGO_PANEDR_HID;
				}
			}
		} else {
			if (bt_link_info->hid_exist &&
			    bt_link_info->a2dp_exist) {
				RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
					 "[BTCoex], BT Profile = HID + A2DP\n");
				algorithm = BT_8822B_1ANT_COEX_ALGO_HID_A2DP;
			} else if (bt_link_info->hid_exist &&
				   bt_link_info->pan_exist) {
				if (bt_hs_on) {
					RT_TRACE(
						rtlpriv, COMP_BT_COEXIST,
						DBG_LOUD,
						"[BTCoex], BT Profile = HID + PAN(HS)\n");
					algorithm =
					    BT_8822B_1ANT_COEX_ALGO_HID_A2DP;
				} else {
					RT_TRACE(
						rtlpriv, COMP_BT_COEXIST,
						DBG_LOUD,
						"[BTCoex], BT Profile = HID + PAN(EDR)\n");
					algorithm =
					    BT_8822B_1ANT_COEX_ALGO_PANEDR_HID;
				}
			} else if (bt_link_info->pan_exist &&
				   bt_link_info->a2dp_exist) {
				if (bt_hs_on) {
					RT_TRACE(
						rtlpriv, COMP_BT_COEXIST,
						DBG_LOUD,
						"[BTCoex], BT Profile = A2DP + PAN(HS)\n");
					algorithm =
					    BT_8822B_1ANT_COEX_ALGO_A2DP_PANHS;
				} else {
					RT_TRACE(
						rtlpriv, COMP_BT_COEXIST,
						DBG_LOUD,
						"[BTCoex], BT Profile = A2DP + PAN(EDR)\n");
					algorithm =
					    BT_8822B_1ANT_COEX_ALGO_PANEDR_A2DP;
				}
			}
		}
	} else if (num_of_diff_profile == 3) {
		if (bt_link_info->sco_exist) {
			if (bt_link_info->hid_exist &&
			    bt_link_info->a2dp_exist) {
				RT_TRACE(
					rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
					"[BTCoex], BT Profile = SCO + HID + A2DP ==> HID\n");
				algorithm = BT_8822B_1ANT_COEX_ALGO_HID;
			} else if (bt_link_info->hid_exist &&
				   bt_link_info->pan_exist) {
				if (bt_hs_on) {
					RT_TRACE(
						rtlpriv, COMP_BT_COEXIST,
						DBG_LOUD,
						"[BTCoex], BT Profile = SCO + HID + PAN(HS)\n");
					algorithm =
					    BT_8822B_1ANT_COEX_ALGO_HID_A2DP;
				} else {
					RT_TRACE(
						rtlpriv, COMP_BT_COEXIST,
						DBG_LOUD,
						"[BTCoex], BT Profile = SCO + HID + PAN(EDR)\n");
					algorithm =
					    BT_8822B_1ANT_COEX_ALGO_PANEDR_HID;
				}
			} else if (bt_link_info->pan_exist &&
				   bt_link_info->a2dp_exist) {
				if (bt_hs_on) {
					RT_TRACE(
						rtlpriv, COMP_BT_COEXIST,
						DBG_LOUD,
						"[BTCoex], BT Profile = SCO + A2DP + PAN(HS)\n");
					algorithm = BT_8822B_1ANT_COEX_ALGO_SCO;
				} else {
					RT_TRACE(
						rtlpriv, COMP_BT_COEXIST,
						DBG_LOUD,
						"[BTCoex], BT Profile = SCO + A2DP + PAN(EDR) ==> HID\n");
					algorithm =
					    BT_8822B_1ANT_COEX_ALGO_PANEDR_HID;
				}
			}
		} else {
			if (bt_link_info->hid_exist &&
			    bt_link_info->pan_exist &&
			    bt_link_info->a2dp_exist) {
				if (bt_hs_on) {
					RT_TRACE(
						rtlpriv, COMP_BT_COEXIST,
						DBG_LOUD,
						"[BTCoex], BT Profile = HID + A2DP + PAN(HS)\n");
					algorithm =
					    BT_8822B_1ANT_COEX_ALGO_HID_A2DP;
				} else {
					RT_TRACE(
						rtlpriv, COMP_BT_COEXIST,
						DBG_LOUD,
						"[BTCoex], BT Profile = HID + A2DP + PAN(EDR)\n");
					algorithm =
					BT_8822B_1ANT_COEX_ALGO_HID_A2DP_PANEDR;
				}
			}
		}
	} else if (num_of_diff_profile >= 3) {
		if (bt_link_info->sco_exist) {
			if (bt_link_info->hid_exist &&
			    bt_link_info->pan_exist &&
			    bt_link_info->a2dp_exist) {
				if (bt_hs_on) {
					RT_TRACE(
						rtlpriv, COMP_BT_COEXIST,
						DBG_LOUD,
						"[BTCoex], Error!!! BT Profile = SCO + HID + A2DP + PAN(HS)\n");

				} else {
					RT_TRACE(
						rtlpriv, COMP_BT_COEXIST,
						DBG_LOUD,
						"[BTCoex], BT Profile = SCO + HID + A2DP + PAN(EDR)==>PAN(EDR)+HID\n");
					algorithm =
					    BT_8822B_1ANT_COEX_ALGO_PANEDR_HID;
				}
			}
		}
	}

	return algorithm;
}

static void halbtc8822b1ant_low_penalty_ra(struct btc_coexist *btcoexist,
					   bool force_exec, bool low_penalty_ra)
{
	coex_dm->cur_low_penalty_ra = low_penalty_ra;

	if (!force_exec) {
		if (coex_dm->pre_low_penalty_ra == coex_dm->cur_low_penalty_ra)
			return;
	}

	if (low_penalty_ra)
		btcoexist->btc_phydm_modify_ra_pcr_threshold(btcoexist, 0, 25);
	else
		btcoexist->btc_phydm_modify_ra_pcr_threshold(btcoexist, 0, 0);

	coex_dm->pre_low_penalty_ra = coex_dm->cur_low_penalty_ra;
}

static void halbtc8822b1ant_write_score_board(struct btc_coexist *btcoexist,
					      u16 bitpos, bool state)
{
	static u16 originalval = 0x8002;

	if (state)
		originalval = originalval | bitpos;
	else
		originalval = originalval & (~bitpos);

	btcoexist->btc_write_2byte(btcoexist, 0xaa, originalval);
}

static void halbtc8822b1ant_read_score_board(struct btc_coexist *btcoexist,
					     u16 *score_board_val)
{
	*score_board_val =
		(btcoexist->btc_read_2byte(btcoexist, 0xaa)) & 0x7fff;
}

static void halbtc8822b1ant_post_state_to_bt(struct btc_coexist *btcoexist,
					     u16 type, bool state)
{
	halbtc8822b1ant_write_score_board(btcoexist, (u16)type, state);
}

static void
halbtc8822b1ant_monitor_bt_enable_disable(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	static u32 bt_disable_cnt;
	bool bt_active = true, bt_disabled = false, wifi_under_5g = false;
	u16 u16tmp;

	/* This function check if bt is disabled */

	/* Read BT on/off status from scoreboard[1],
	 * enable this only if BT patch support this feature
	 */
	halbtc8822b1ant_read_score_board(btcoexist, &u16tmp);

	bt_active = u16tmp & BIT(1);

	if (bt_active) {
		bt_disable_cnt = 0;
		bt_disabled = false;
		btcoexist->btc_set(btcoexist, BTC_SET_BL_BT_DISABLE,
				   &bt_disabled);
	} else {
		bt_disable_cnt++;
		if (bt_disable_cnt >= 2) {
			bt_disabled = true;
			bt_disable_cnt = 2;
		}

		btcoexist->btc_set(btcoexist, BTC_SET_BL_BT_DISABLE,
				   &bt_disabled);
	}

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_UNDER_5G, &wifi_under_5g);

	if ((wifi_under_5g) || (bt_disabled))
		halbtc8822b1ant_low_penalty_ra(btcoexist, NORMAL_EXEC, false);
	else
		halbtc8822b1ant_low_penalty_ra(btcoexist, NORMAL_EXEC, true);

	if (coex_sta->bt_disabled != bt_disabled) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], BT is from %s to %s!!\n",
			 (coex_sta->bt_disabled ? "disabled" : "enabled"),
			 (bt_disabled ? "disabled" : "enabled"));
		coex_sta->bt_disabled = bt_disabled;
	}
}

static void halbtc8822b1ant_enable_gnt_to_gpio(struct btc_coexist *btcoexist,
					       bool isenable)
{
	static u8 bit_val[5] = {0, 0, 0, 0, 0};
	static bool state;

	if (!btcoexist->dbg_mode_1ant)
		return;

	if (state == isenable)
		return;

	state = isenable;

	if (isenable) {
		/* enable GNT_WL, GNT_BT to GPIO for debug */
		btcoexist->btc_write_1byte_bitmask(btcoexist, 0x73, 0x8, 0x1);

		/* store original value */
		bit_val[0] =
			(btcoexist->btc_read_1byte(btcoexist, 0x66) & BIT(4)) >>
			4; /*0x66[4] */
		bit_val[1] = (btcoexist->btc_read_1byte(btcoexist, 0x67) &
			      BIT(0)); /*0x66[8] */
		bit_val[2] =
			(btcoexist->btc_read_1byte(btcoexist, 0x42) & BIT(3)) >>
			3; /*0x40[19] */
		bit_val[3] =
			(btcoexist->btc_read_1byte(btcoexist, 0x65) & BIT(7)) >>
			7; /*0x64[15] */
		bit_val[4] =
			(btcoexist->btc_read_1byte(btcoexist, 0x72) & BIT(2)) >>
			2; /*0x70[18] */

		/*  switch GPIO Mux */
		btcoexist->btc_write_1byte_bitmask(btcoexist, 0x66, BIT(4),
						   0x0); /*0x66[4] = 0 */
		btcoexist->btc_write_1byte_bitmask(btcoexist, 0x67, BIT(0),
						   0x0); /*0x66[8] = 0 */
		btcoexist->btc_write_1byte_bitmask(btcoexist, 0x42, BIT(3),
						   0x0); /*0x40[19] = 0 */
		btcoexist->btc_write_1byte_bitmask(btcoexist, 0x65, BIT(7),
						   0x0); /*0x64[15] = 0 */
		btcoexist->btc_write_1byte_bitmask(btcoexist, 0x72, BIT(2),
						   0x0); /*0x70[18] = 0 */

	} else {
		btcoexist->btc_write_1byte_bitmask(btcoexist, 0x73, 0x8, 0x0);

		/*  Restore original value  */
		/*  switch GPIO Mux */
		btcoexist->btc_write_1byte_bitmask(btcoexist, 0x66, BIT(4),
						   bit_val[0]); /*0x66[4] = 0 */
		btcoexist->btc_write_1byte_bitmask(btcoexist, 0x67, BIT(0),
						   bit_val[1]); /*0x66[8] = 0 */
		btcoexist->btc_write_1byte_bitmask(
			btcoexist, 0x42, BIT(3), bit_val[2]); /*0x40[19] = 0 */
		btcoexist->btc_write_1byte_bitmask(
			btcoexist, 0x65, BIT(7), bit_val[3]); /*0x64[15] = 0 */
		btcoexist->btc_write_1byte_bitmask(
			btcoexist, 0x72, BIT(2), bit_val[4]); /*0x70[18] = 0 */
	}
}

static u32
halbtc8822b1ant_ltecoex_indirect_read_reg(struct btc_coexist *btcoexist,
					  u16 reg_addr)
{
	u32 delay_count = 0;

	/* wait for ready bit before access 0x1700 */
	while (1) {
		if ((btcoexist->btc_read_1byte(btcoexist, 0x1703) & BIT(5)) ==
		    0) {
			mdelay(50);
			delay_count++;
			if (delay_count >= 10) {
				delay_count = 0;
				break;
			}
		} else {
			break;
		}
	}

	btcoexist->btc_write_4byte(btcoexist, 0x1700, 0x800F0000 | reg_addr);

	return btcoexist->btc_read_4byte(btcoexist, 0x1708); /* get read data */
}

static void
halbtc8822b1ant_ltecoex_indirect_write_reg(struct btc_coexist *btcoexist,
					   u16 reg_addr, u32 bit_mask,
					   u32 reg_value)
{
	u32 val, i = 0, bitpos = 0, delay_count = 0;

	if (bit_mask == 0x0)
		return;

	if (bit_mask == 0xffffffff) {
		/* wait for ready bit before access 0x1700/0x1704 */
		while (1) {
			if ((btcoexist->btc_read_1byte(btcoexist, 0x1703) &
			     BIT(5)) == 0) {
				mdelay(50);
				delay_count++;
				if (delay_count >= 10) {
					delay_count = 0;
					break;
				}
			} else {
				break;
			}
		}

		btcoexist->btc_write_4byte(btcoexist, 0x1704,
					   reg_value); /* put write data */

		btcoexist->btc_write_4byte(btcoexist, 0x1700,
					   0xc00F0000 | reg_addr);
	} else {
		for (i = 0; i <= 31; i++) {
			if (((bit_mask >> i) & 0x1) == 0x1) {
				bitpos = i;
				break;
			}
		}

		/* read back register value before write */
		val = halbtc8822b1ant_ltecoex_indirect_read_reg(btcoexist,
								reg_addr);
		val = (val & (~bit_mask)) | (reg_value << bitpos);

		/* wait for ready bit before access 0x1700/0x1704 */
		while (1) {
			if ((btcoexist->btc_read_1byte(btcoexist, 0x1703) &
			     BIT(5)) == 0) {
				mdelay(50);
				delay_count++;
				if (delay_count >= 10) {
					delay_count = 0;
					break;
				}
			} else {
				break;
			}
		}

		btcoexist->btc_write_4byte(btcoexist, 0x1704,
					   val); /* put write data */

		btcoexist->btc_write_4byte(btcoexist, 0x1700,
					   0xc00F0000 | reg_addr);
	}
}

static void halbtc8822b1ant_ltecoex_enable(struct btc_coexist *btcoexist,
					   bool enable)
{
	u8 val;

	val = (enable) ? 1 : 0;
	/* 0x38[7] */
	halbtc8822b1ant_ltecoex_indirect_write_reg(btcoexist, 0x38, 0x80, val);
}

static void
halbtc8822b1ant_ltecoex_pathcontrol_owner(struct btc_coexist *btcoexist,
					  bool wifi_control)
{
	u8 val;

	val = (wifi_control) ? 1 : 0;
	/* 0x70[26] */
	btcoexist->btc_write_1byte_bitmask(btcoexist, 0x73, 0x4, val);
}

static void halbtc8822b1ant_ltecoex_set_gnt_bt(struct btc_coexist *btcoexist,
					       u8 control_block,
					       bool sw_control, u8 state)
{
	u32 val = 0, bit_mask;

	state = state & 0x1;
	/*LTE indirect 0x38=0xccxx (sw : gnt_wl=1,sw gnt_bt=1)
	 *0x38=0xddxx (sw : gnt_bt=1 , sw gnt_wl=0)
	 *0x38=0x55xx(hw pta :gnt_wl /gnt_bt )
	 */
	val = (sw_control) ? ((state << 1) | 0x1) : 0;

	switch (control_block) {
	case BT_8822B_1ANT_GNT_BLOCK_RFC_BB:
	default:
		bit_mask = 0xc000;
		halbtc8822b1ant_ltecoex_indirect_write_reg(
			btcoexist, 0x38, bit_mask, val); /* 0x38[15:14] */
		bit_mask = 0x0c00;
		halbtc8822b1ant_ltecoex_indirect_write_reg(
			btcoexist, 0x38, bit_mask, val); /* 0x38[11:10] */
		break;
	case BT_8822B_1ANT_GNT_BLOCK_RFC:
		bit_mask = 0xc000;
		halbtc8822b1ant_ltecoex_indirect_write_reg(
			btcoexist, 0x38, bit_mask, val); /* 0x38[15:14] */
		break;
	case BT_8822B_1ANT_GNT_BLOCK_BB:
		bit_mask = 0x0c00;
		halbtc8822b1ant_ltecoex_indirect_write_reg(
			btcoexist, 0x38, bit_mask, val); /* 0x38[11:10] */
		break;
	}
}

static void halbtc8822b1ant_ltecoex_set_gnt_wl(struct btc_coexist *btcoexist,
					       u8 control_block,
					       bool sw_control, u8 state)
{
	u32 val = 0, bit_mask;
	/*LTE indirect 0x38=0xccxx (sw : gnt_wl=1,sw gnt_bt=1)
	 *0x38=0xddxx (sw : gnt_bt=1 , sw gnt_wl=0)
	 *0x38=0x55xx(hw pta :gnt_wl /gnt_bt )
	 */

	state = state & 0x1;
	val = (sw_control) ? ((state << 1) | 0x1) : 0;

	switch (control_block) {
	case BT_8822B_1ANT_GNT_BLOCK_RFC_BB:
	default:
		bit_mask = 0x3000;
		halbtc8822b1ant_ltecoex_indirect_write_reg(
			btcoexist, 0x38, bit_mask, val); /* 0x38[13:12] */
		bit_mask = 0x0300;
		halbtc8822b1ant_ltecoex_indirect_write_reg(
			btcoexist, 0x38, bit_mask, val); /* 0x38[9:8] */
		break;
	case BT_8822B_1ANT_GNT_BLOCK_RFC:
		bit_mask = 0x3000;
		halbtc8822b1ant_ltecoex_indirect_write_reg(
			btcoexist, 0x38, bit_mask, val); /* 0x38[13:12] */
		break;
	case BT_8822B_1ANT_GNT_BLOCK_BB:
		bit_mask = 0x0300;
		halbtc8822b1ant_ltecoex_indirect_write_reg(
			btcoexist, 0x38, bit_mask, val); /* 0x38[9:8] */
		break;
	}
}

static void
halbtc8822b1ant_ltecoex_set_coex_table(struct btc_coexist *btcoexist,
				       u8 table_type, u16 table_content)
{
	u16 reg_addr = 0x0000;

	switch (table_type) {
	case BT_8822B_1ANT_CTT_WL_VS_LTE:
		reg_addr = 0xa0;
		break;
	case BT_8822B_1ANT_CTT_BT_VS_LTE:
		reg_addr = 0xa4;
		break;
	}

	if (reg_addr != 0x0000)
		halbtc8822b1ant_ltecoex_indirect_write_reg(
			btcoexist, reg_addr, 0xffff,
			table_content); /* 0xa0[15:0] or 0xa4[15:0] */
}

static void halbtc8822b1ant_set_wltoggle_coex_table(
	struct btc_coexist *btcoexist, bool force_exec, u8 interval,
	u8 val0x6c4_b0, u8 val0x6c4_b1, u8 val0x6c4_b2, u8 val0x6c4_b3)
{
	static u8 pre_h2c_parameter[6] = {0};
	u8 cur_h2c_parameter[6] = {0};
	u8 i, match_cnt = 0;

	cur_h2c_parameter[0] = 0x7; /* op_code, 0x7= wlan toggle slot*/

	cur_h2c_parameter[1] = interval;
	cur_h2c_parameter[2] = val0x6c4_b0;
	cur_h2c_parameter[3] = val0x6c4_b1;
	cur_h2c_parameter[4] = val0x6c4_b2;
	cur_h2c_parameter[5] = val0x6c4_b3;

	if (!force_exec) {
		for (i = 1; i <= 5; i++) {
			if (cur_h2c_parameter[i] != pre_h2c_parameter[i])
				break;

			match_cnt++;
		}

		if (match_cnt == 5)
			return;
	}

	for (i = 1; i <= 5; i++)
		pre_h2c_parameter[i] = cur_h2c_parameter[i];

	btcoexist->btc_fill_h2c(btcoexist, 0x69, 6, cur_h2c_parameter);
}

static void halbtc8822b1ant_set_coex_table(struct btc_coexist *btcoexist,
					   u32 val0x6c0, u32 val0x6c4,
					   u32 val0x6c8, u8 val0x6cc)
{
	btcoexist->btc_write_4byte(btcoexist, 0x6c0, val0x6c0);

	btcoexist->btc_write_4byte(btcoexist, 0x6c4, val0x6c4);

	btcoexist->btc_write_4byte(btcoexist, 0x6c8, val0x6c8);

	btcoexist->btc_write_1byte(btcoexist, 0x6cc, val0x6cc);
}

static void halbtc8822b1ant_coex_table(struct btc_coexist *btcoexist,
				       bool force_exec, u32 val0x6c0,
				       u32 val0x6c4, u32 val0x6c8, u8 val0x6cc)
{
	coex_dm->cur_val0x6c0 = val0x6c0;
	coex_dm->cur_val0x6c4 = val0x6c4;
	coex_dm->cur_val0x6c8 = val0x6c8;
	coex_dm->cur_val0x6cc = val0x6cc;

	if (!force_exec) {
		if ((coex_dm->pre_val0x6c0 == coex_dm->cur_val0x6c0) &&
		    (coex_dm->pre_val0x6c4 == coex_dm->cur_val0x6c4) &&
		    (coex_dm->pre_val0x6c8 == coex_dm->cur_val0x6c8) &&
		    (coex_dm->pre_val0x6cc == coex_dm->cur_val0x6cc))
			return;
	}
	halbtc8822b1ant_set_coex_table(btcoexist, val0x6c0, val0x6c4, val0x6c8,
				       val0x6cc);

	coex_dm->pre_val0x6c0 = coex_dm->cur_val0x6c0;
	coex_dm->pre_val0x6c4 = coex_dm->cur_val0x6c4;
	coex_dm->pre_val0x6c8 = coex_dm->cur_val0x6c8;
	coex_dm->pre_val0x6cc = coex_dm->cur_val0x6cc;
}

static void halbtc8822b1ant_coex_table_with_type(struct btc_coexist *btcoexist,
						 bool force_exec, u8 type)
{
	u32 break_table;
	u8 select_table;

	coex_sta->coex_table_type = type;

	if (coex_sta->concurrent_rx_mode_on) {
		break_table = 0xf0ffffff; /* set WL hi-pri can break BT */
		select_table = 0x3; /* set Tx response = Hi-Pri
				     * (ex: Transmitting ACK,BA,CTS)
				     */
	} else {
		break_table = 0xffffff;
		select_table = 0x3;
	}

	switch (type) {
	case 0:
		halbtc8822b1ant_coex_table(btcoexist, force_exec, 0x55555555,
					   0x55555555, break_table,
					   select_table);
		break;
	case 1:
		halbtc8822b1ant_coex_table(btcoexist, force_exec, 0x55555555,
					   0x5a5a5a5a, break_table,
					   select_table);
		break;
	case 2:
		halbtc8822b1ant_coex_table(btcoexist, force_exec, 0xaa5a5a5a,
					   0xaa5a5a5a, break_table,
					   select_table);
		break;
	case 3:
		halbtc8822b1ant_coex_table(btcoexist, force_exec, 0x55555555,
					   0xaa5a5a5a, break_table,
					   select_table);
		break;
	case 4:
		halbtc8822b1ant_coex_table(btcoexist, force_exec, 0xaa555555,
					   0xaa5a5a5a, break_table,
					   select_table);
		break;
	case 5:
		halbtc8822b1ant_coex_table(btcoexist, force_exec, 0x5a5a5a5a,
					   0x5a5a5a5a, break_table,
					   select_table);
		break;
	case 6:
		halbtc8822b1ant_coex_table(btcoexist, force_exec, 0x55555555,
					   0xaaaaaaaa, break_table,
					   select_table);
		break;
	case 7:
		halbtc8822b1ant_coex_table(btcoexist, force_exec, 0xaaaaaaaa,
					   0xaaaaaaaa, break_table,
					   select_table);
		break;
	case 8:
		halbtc8822b1ant_coex_table(btcoexist, force_exec, 0xffffffff,
					   0xffffffff, break_table,
					   select_table);
		break;
	case 9:
		halbtc8822b1ant_coex_table(btcoexist, force_exec, 0x5a5a5555,
					   0xaaaa5a5a, break_table,
					   select_table);
		break;
	case 10:
		halbtc8822b1ant_coex_table(btcoexist, force_exec, 0xaaaa5aaa,
					   0xaaaa5aaa, break_table,
					   select_table);
		break;
	case 11:
		halbtc8822b1ant_coex_table(btcoexist, force_exec, 0xaaaaa5aa,
					   0xaaaaaaaa, break_table,
					   select_table);
		break;
	case 12:
		halbtc8822b1ant_coex_table(btcoexist, force_exec, 0xaaaaa5aa,
					   0xaaaaa5aa, break_table,
					   select_table);
		break;
	case 13:
		halbtc8822b1ant_coex_table(btcoexist, force_exec, 0x55555555,
					   0xaaaa5a5a, break_table,
					   select_table);
		break;
	case 14:
		halbtc8822b1ant_coex_table(btcoexist, force_exec, 0x5a5a555a,
					   0xaaaa5a5a, break_table,
					   select_table);
		break;
	case 15:
		halbtc8822b1ant_coex_table(btcoexist, force_exec, 0x55555555,
					   0xaaaa55aa, break_table,
					   select_table);
		break;
	case 16:
		halbtc8822b1ant_coex_table(btcoexist, force_exec, 0x5a5a555a,
					   0x5a5a555a, break_table,
					   select_table);
		break;
	case 17:
		halbtc8822b1ant_coex_table(btcoexist, force_exec, 0xaaaa55aa,
					   0xaaaa55aa, break_table,
					   select_table);
		break;
	case 18:
		halbtc8822b1ant_coex_table(btcoexist, force_exec, 0x55555555,
					   0x5aaa5a5a, break_table,
					   select_table);
		break;
	case 19:
		halbtc8822b1ant_coex_table(btcoexist, force_exec, 0xa5555555,
					   0xaaaa5aaa, break_table,
					   select_table);
		break;
	case 20:
		halbtc8822b1ant_coex_table(btcoexist, force_exec, 0x55555555,
					   0xaaaa5aaa, break_table,
					   select_table);
		break;
	default:
		break;
	}
}

static void
halbtc8822b1ant_set_fw_ignore_wlan_act(struct btc_coexist *btcoexist,
				       bool enable)
{
	u8 h2c_parameter[1] = {0};

	if (enable)
		h2c_parameter[0] |= BIT(0); /* function enable */

	btcoexist->btc_fill_h2c(btcoexist, 0x63, 1, h2c_parameter);
}

static void halbtc8822b1ant_ignore_wlan_act(struct btc_coexist *btcoexist,
					    bool force_exec, bool enable)
{
	coex_dm->cur_ignore_wlan_act = enable;

	if (!force_exec) {
		if (coex_dm->pre_ignore_wlan_act ==
		    coex_dm->cur_ignore_wlan_act) {
			coex_dm->pre_ignore_wlan_act =
				coex_dm->cur_ignore_wlan_act;
			return;
		}
	}

	halbtc8822b1ant_set_fw_ignore_wlan_act(btcoexist, enable);

	coex_dm->pre_ignore_wlan_act = coex_dm->cur_ignore_wlan_act;
}

static void halbtc8822b1ant_set_lps_rpwm(struct btc_coexist *btcoexist,
					 u8 lps_val, u8 rpwm_val)
{
	u8 lps = lps_val;
	u8 rpwm = rpwm_val;

	btcoexist->btc_set(btcoexist, BTC_SET_U1_LPS_VAL, &lps);
	btcoexist->btc_set(btcoexist, BTC_SET_U1_RPWM_VAL, &rpwm);
}

static void halbtc8822b1ant_lps_rpwm(struct btc_coexist *btcoexist,
				     bool force_exec, u8 lps_val, u8 rpwm_val)
{
	coex_dm->cur_lps = lps_val;
	coex_dm->cur_rpwm = rpwm_val;

	if (!force_exec) {
		if ((coex_dm->pre_lps == coex_dm->cur_lps) &&
		    (coex_dm->pre_rpwm == coex_dm->cur_rpwm))
			return;
	}
	halbtc8822b1ant_set_lps_rpwm(btcoexist, lps_val, rpwm_val);

	coex_dm->pre_lps = coex_dm->cur_lps;
	coex_dm->pre_rpwm = coex_dm->cur_rpwm;
}

static void halbtc8822b1ant_ps_tdma_check_for_power_save_state(
	struct btc_coexist *btcoexist, bool new_ps_state)
{
	u8 lps_mode = 0x0;
	u8 h2c_parameter[5] = {0x8, 0, 0, 0, 0};

	btcoexist->btc_get(btcoexist, BTC_GET_U1_LPS_MODE, &lps_mode);

	if (lps_mode) { /* already under LPS state */
		if (new_ps_state) {
			/* keep state under LPS, do nothing. */
		} else {
			/* will leave LPS state, turn off psTdma first */

			btcoexist->btc_fill_h2c(btcoexist, 0x60, 5,
						h2c_parameter);
		}
	} else { /* NO PS state */
		if (new_ps_state) {
			/* will enter LPS state, turn off psTdma first */

			btcoexist->btc_fill_h2c(btcoexist, 0x60, 5,
						h2c_parameter);
		} else {
			/* keep state under NO PS state, do nothing. */
		}
	}
}

static bool halbtc8822b1ant_power_save_state(struct btc_coexist *btcoexist,
					     u8 ps_type, u8 lps_val,
					     u8 rpwm_val)
{
	bool low_pwr_disable = false, result = true;

	switch (ps_type) {
	case BTC_PS_WIFI_NATIVE:
		/* recover to original 32k low power setting */
		coex_sta->force_lps_ctrl = false;
		low_pwr_disable = false;
		btcoexist->btc_set(btcoexist, BTC_SET_ACT_DISABLE_LOW_POWER,
				   &low_pwr_disable);
		btcoexist->btc_set(btcoexist, BTC_SET_ACT_NORMAL_LPS, NULL);
		break;
	case BTC_PS_LPS_ON:

		coex_sta->force_lps_ctrl = true;
		halbtc8822b1ant_ps_tdma_check_for_power_save_state(btcoexist,
								   true);
		halbtc8822b1ant_lps_rpwm(btcoexist, NORMAL_EXEC, lps_val,
					 rpwm_val);
		/* when coex force to enter LPS, do not enter 32k low power. */
		low_pwr_disable = true;
		btcoexist->btc_set(btcoexist, BTC_SET_ACT_DISABLE_LOW_POWER,
				   &low_pwr_disable);
		/* power save must executed before psTdma. */
		btcoexist->btc_set(btcoexist, BTC_SET_ACT_ENTER_LPS, NULL);

		break;
	case BTC_PS_LPS_OFF:

		coex_sta->force_lps_ctrl = true;
		halbtc8822b1ant_ps_tdma_check_for_power_save_state(btcoexist,
								   false);
		result = btcoexist->btc_set(btcoexist, BTC_SET_ACT_LEAVE_LPS,
					    NULL);

		break;
	default:
		break;
	}

	return result;
}

static void halbtc8822b1ant_set_fw_pstdma(struct btc_coexist *btcoexist,
					  u8 byte1, u8 byte2, u8 byte3,
					  u8 byte4, u8 byte5)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	u8 h2c_parameter[5] = {0};
	u8 real_byte1 = byte1, real_byte5 = byte5;
	bool ap_enable = false, result = false;
	struct btc_bt_link_info *bt_link_info = &btcoexist->bt_link_info;

	if (byte5 & BIT(2))
		coex_sta->is_tdma_btautoslot = true;
	else
		coex_sta->is_tdma_btautoslot = false;

	/* release bt-auto slot for auto-slot hang is detected!! */
	if (coex_sta->is_tdma_btautoslot)
		if ((coex_sta->is_tdma_btautoslot_hang) ||
		    (bt_link_info->slave_role))
			byte5 = byte5 & 0xfb;

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_AP_MODE_ENABLE,
			   &ap_enable);

	if ((ap_enable) && (byte1 & BIT(4) && !(byte1 & BIT(5)))) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], %s == FW for 1Ant AP mode\n", __func__);

		real_byte1 &= ~BIT(4);
		real_byte1 |= BIT(5);

		real_byte5 |= BIT(5);
		real_byte5 &= ~BIT(6);

		halbtc8822b1ant_power_save_state(btcoexist, BTC_PS_WIFI_NATIVE,
						 0x0, 0x0);

	} else if (byte1 & BIT(4) && !(byte1 & BIT(5))) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], %s == Force LPS (byte1 = 0x%x)\n",
			 __func__, byte1);
		if (!halbtc8822b1ant_power_save_state(btcoexist, BTC_PS_LPS_OFF,
						      0x50, 0x4))
			result = true;
	} else {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], %s == native power save (byte1 = 0x%x)\n",
			 __func__, byte1);
		halbtc8822b1ant_power_save_state(btcoexist, BTC_PS_WIFI_NATIVE,
						 0x0, 0x0);
	}

	coex_sta->is_set_ps_state_fail = result;

	if (!coex_sta->is_set_ps_state_fail) {
		h2c_parameter[0] = real_byte1;
		h2c_parameter[1] = byte2;
		h2c_parameter[2] = byte3;
		h2c_parameter[3] = byte4;
		h2c_parameter[4] = real_byte5;

		coex_dm->ps_tdma_para[0] = real_byte1;
		coex_dm->ps_tdma_para[1] = byte2;
		coex_dm->ps_tdma_para[2] = byte3;
		coex_dm->ps_tdma_para[3] = byte4;
		coex_dm->ps_tdma_para[4] = real_byte5;

		btcoexist->btc_fill_h2c(btcoexist, 0x60, 5, h2c_parameter);

	} else {
		coex_sta->cnt_set_ps_state_fail++;
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], %s == Force Leave LPS Fail (cnt = %d)\n",
			 __func__, coex_sta->cnt_set_ps_state_fail);
	}
}

static void halbtc8822b1ant_ps_tdma(struct btc_coexist *btcoexist,
				    bool force_exec, bool turn_on, u8 type)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	struct btc_bt_link_info *bt_link_info = &btcoexist->bt_link_info;
	bool wifi_busy = false;
	static u8 ps_tdma_byte4_modify, pre_ps_tdma_byte4_modify;
	static bool pre_wifi_busy;

	coex_dm->cur_ps_tdma_on = turn_on;
	coex_dm->cur_ps_tdma = type;

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_BUSY, &wifi_busy);

	if (wifi_busy != pre_wifi_busy) {
		force_exec = true;
		pre_wifi_busy = wifi_busy;
	}

	/* 0x778 = 0x1 at wifi slot (no blocking BT Low-Pri pkts) */
	if (bt_link_info->slave_role)
		ps_tdma_byte4_modify = 0x1;
	else
		ps_tdma_byte4_modify = 0x0;

	if (pre_ps_tdma_byte4_modify != ps_tdma_byte4_modify) {
		force_exec = true;
		pre_ps_tdma_byte4_modify = ps_tdma_byte4_modify;
	}

	if (!force_exec) {
		if ((coex_dm->pre_ps_tdma_on == coex_dm->cur_ps_tdma_on) &&
		    (coex_dm->pre_ps_tdma == coex_dm->cur_ps_tdma)) {
			RT_TRACE(
				rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				"[BTCoex], Skip TDMA because no change TDMA(%s, %d)\n",
				(coex_dm->cur_ps_tdma_on ? "on" : "off"),
				coex_dm->cur_ps_tdma);
			return;
		}
	}

	if (coex_dm->cur_ps_tdma_on) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], ********** TDMA(on, %d) **********\n",
			 coex_dm->cur_ps_tdma);

		btcoexist->btc_write_1byte_bitmask(
			btcoexist, 0x550, 0x8, 0x1); /* enable TBTT nterrupt */
	} else {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], ********** TDMA(off, %d) **********\n",
			 coex_dm->cur_ps_tdma);
	}

	if (turn_on) {
		/* enable TBTT nterrupt */
		btcoexist->btc_write_1byte_bitmask(btcoexist, 0x550, 0x8, 0x1);

		switch (type) {
		default:
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x61, 0x35,
						      0x03, 0x11, 0x11);
			break;
		case 1:
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x61, 0x3a,
						      0x03, 0x11, 0x10);
			break;
		case 3:
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x51, 0x30,
						      0x03, 0x10, 0x50);
			break;
		case 4:
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x51, 0x21,
						      0x03, 0x10, 0x50);
			break;
		case 5:
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x61, 0x15,
						      0x3, 0x11, 0x11);
			break;
		case 6:
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x61, 0x20,
						      0x3, 0x11, 0x10);
			break;
		case 7:
			halbtc8822b1ant_set_fw_pstdma(
				btcoexist, 0x51, 0x10, 0x03, 0x10,
				0x54 | ps_tdma_byte4_modify);
			break;
		case 8:
			halbtc8822b1ant_set_fw_pstdma(
				btcoexist, 0x51, 0x10, 0x03, 0x10,
				0x14 | ps_tdma_byte4_modify);
			break;
		case 11:
			halbtc8822b1ant_set_fw_pstdma(
				btcoexist, 0x61, 0x25, 0x03, 0x11,
				0x10 | ps_tdma_byte4_modify);
			break;
		case 12:
			halbtc8822b1ant_set_fw_pstdma(
				btcoexist, 0x51, 0x30, 0x03, 0x10,
				0x50 | ps_tdma_byte4_modify);
			break;
		case 13:
			halbtc8822b1ant_set_fw_pstdma(
				btcoexist, 0x51, 0x10, 0x07, 0x10,
				0x54 | ps_tdma_byte4_modify);
			break;
		case 14:
			halbtc8822b1ant_set_fw_pstdma(
				btcoexist, 0x51, 0x15, 0x03, 0x10,
				0x50 | ps_tdma_byte4_modify);
			break;
		case 15:
			halbtc8822b1ant_set_fw_pstdma(
				btcoexist, 0x51, 0x20, 0x03, 0x10,
				0x10 | ps_tdma_byte4_modify);
			break;
		case 17:
			halbtc8822b1ant_set_fw_pstdma(
				btcoexist, 0x61, 0x10, 0x03, 0x11,
				0x14 | ps_tdma_byte4_modify);
			break;
		case 18:
			halbtc8822b1ant_set_fw_pstdma(
				btcoexist, 0x51, 0x10, 0x03, 0x10,
				0x50 | ps_tdma_byte4_modify);
			break;

		case 20:
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x61, 0x30,
						      0x03, 0x11, 0x10);
			break;
		case 22:
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x61, 0x25,
						      0x03, 0x11, 0x10);
			break;
		case 27:
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x61, 0x10,
						      0x03, 0x11, 0x15);
			break;
		case 32:
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x61, 0x35,
						      0x3, 0x11, 0x11);
			break;
		case 33:
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x61, 0x35,
						      0x03, 0x11, 0x10);
			break;
		case 41:
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x51, 0x45,
						      0x3, 0x11, 0x11);
			break;
		case 42:
			halbtc8822b1ant_set_fw_pstdma(
				btcoexist, 0x51, 0x1e, 0x3, 0x10,
				0x14 | ps_tdma_byte4_modify);
			break;
		case 43:
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x51, 0x45,
						      0x3, 0x10, 0x14);
			break;
		case 44:
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x51, 0x25,
						      0x3, 0x10, 0x10);
			break;
		case 45:
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x51, 0x29,
						      0x3, 0x10, 0x10);
			break;
		case 46:
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x51, 0x1a,
						      0x3, 0x10, 0x10);
			break;
		case 47:
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x51, 0x32,
						      0x3, 0x10, 0x10);
			break;
		case 48:
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x51, 0x29,
						      0x3, 0x10, 0x10);
			break;
		case 49:
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x55, 0x10,
						      0x3, 0x10, 0x54);
			break;
		case 50:
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x51, 0x4a,
						      0x3, 0x10, 0x10);
			break;
		case 51:
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x61, 0x35,
						      0x3, 0x10, 0x11);
			break;
		}
	} else {
		switch (type) {
		case 0:
		default: /* Software control, Antenna at BT side */
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x0, 0x0, 0x0,
						      0x0, 0x0);
			break;
		case 8: /* PTA Control */
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x8, 0x0, 0x0,
						      0x0, 0x0);
			break;
		case 9: /* Software control, Antenna at WiFi side */
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x0, 0x0, 0x0,
						      0x0, 0x0);
			break;
		case 10: /* under 5G , 0x778=1*/
			halbtc8822b1ant_set_fw_pstdma(btcoexist, 0x0, 0x0, 0x0,
						      0x0, 0x0);

			break;
		}
	}

	if (!coex_sta->is_set_ps_state_fail) {
		/* update pre state */
		coex_dm->pre_ps_tdma_on = coex_dm->cur_ps_tdma_on;
		coex_dm->pre_ps_tdma = coex_dm->cur_ps_tdma;
	}
}

static void halbtc8822b1ant_sw_mechanism(struct btc_coexist *btcoexist,
					 bool low_penalty_ra)
{
	halbtc8822b1ant_low_penalty_ra(btcoexist, NORMAL_EXEC, low_penalty_ra);
}

/* rf4 type by efuse, and for ant at main aux inverse use,
 * because is 2x2, and control types are the same, does not need
 */

static void halbtc8822b1ant_set_rfe_type(struct btc_coexist *btcoexist)
{
	struct btc_board_info *board_info = &btcoexist->board_info;

	/* Ext switch buffer mux */
	btcoexist->btc_write_1byte(btcoexist, 0x974, 0xff);
	btcoexist->btc_write_1byte_bitmask(btcoexist, 0x1991, 0x3, 0x0);
	btcoexist->btc_write_1byte_bitmask(btcoexist, 0xcbe, 0x8, 0x0);

	/* the following setup should be got from Efuse in the future */
	rfe_type->rfe_module_type = board_info->rfe_type;

	rfe_type->ext_ant_switch_ctrl_polarity = 0;

	switch (rfe_type->rfe_module_type) {
	case 0:
	default:
		rfe_type->ext_ant_switch_exist = true;
		rfe_type->ext_ant_switch_type =
			BT_8822B_1ANT_EXT_ANT_SWITCH_USE_SPDT;
		break;
	case 1:
		rfe_type->ext_ant_switch_exist = true;
		rfe_type->ext_ant_switch_type =
			BT_8822B_1ANT_EXT_ANT_SWITCH_USE_SPDT;
		break;
	case 2:
		rfe_type->ext_ant_switch_exist = true;
		rfe_type->ext_ant_switch_type =
			BT_8822B_1ANT_EXT_ANT_SWITCH_USE_SPDT;
		break;
	case 3:
		rfe_type->ext_ant_switch_exist = true;
		rfe_type->ext_ant_switch_type =
			BT_8822B_1ANT_EXT_ANT_SWITCH_USE_SPDT;
		break;
	case 4:
		rfe_type->ext_ant_switch_exist = true;
		rfe_type->ext_ant_switch_type =
			BT_8822B_1ANT_EXT_ANT_SWITCH_USE_SPDT;
		break;
	case 5:
		rfe_type->ext_ant_switch_exist = true;
		rfe_type->ext_ant_switch_type =
			BT_8822B_1ANT_EXT_ANT_SWITCH_USE_SPDT;
		break;
	case 6:
		rfe_type->ext_ant_switch_exist = true;
		rfe_type->ext_ant_switch_type =
			BT_8822B_1ANT_EXT_ANT_SWITCH_USE_SPDT;
		break;
	case 7:
		rfe_type->ext_ant_switch_exist = true;
		rfe_type->ext_ant_switch_type =
			BT_8822B_1ANT_EXT_ANT_SWITCH_USE_SPDT;
		break;
	}
}

/*anttenna control by bb mac bt antdiv pta to write 0x4c 0xcb4,0xcbd*/

static void halbtc8822b1ant_set_ext_ant_switch(struct btc_coexist *btcoexist,
					       bool force_exec, u8 ctrl_type,
					       u8 pos_type)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	bool switch_polatiry_inverse = false;
	u8 regval_0xcbd = 0, regval_0x64;
	u32 u32tmp1 = 0, u32tmp2 = 0, u32tmp3 = 0;

	/* Ext switch buffer mux */
	btcoexist->btc_write_1byte(btcoexist, 0x974, 0xff);
	btcoexist->btc_write_1byte_bitmask(btcoexist, 0x1991, 0x3, 0x0);
	btcoexist->btc_write_1byte_bitmask(btcoexist, 0xcbe, 0x8, 0x0);

	if (!rfe_type->ext_ant_switch_exist)
		return;

	coex_dm->cur_ext_ant_switch_status = (ctrl_type << 8) + pos_type;

	if (!force_exec) {
		if (coex_dm->pre_ext_ant_switch_status ==
		    coex_dm->cur_ext_ant_switch_status)
			return;
	}

	coex_dm->pre_ext_ant_switch_status = coex_dm->cur_ext_ant_switch_status;

	/* swap control polarity if use different switch control polarity*/
	/* Normal switch polarity for SPDT,
	 * 0xcbd[1:0] = 2b'01 => Ant to BTG,
	 * 0xcbd[1:0] = 2b'10 => Ant to WLG
	 */
	switch_polatiry_inverse = (rfe_type->ext_ant_switch_ctrl_polarity == 1 ?
					   ~switch_polatiry_inverse :
					   switch_polatiry_inverse);

	switch (pos_type) {
	default:
	case BT_8822B_1ANT_EXT_ANT_SWITCH_TO_BT:
	case BT_8822B_1ANT_EXT_ANT_SWITCH_TO_NOCARE:

		break;
	case BT_8822B_1ANT_EXT_ANT_SWITCH_TO_WLG:
		break;
	case BT_8822B_1ANT_EXT_ANT_SWITCH_TO_WLA:
		break;
	}

	if (rfe_type->ext_ant_switch_type ==
	    BT_8822B_1ANT_EXT_ANT_SWITCH_USE_SPDT) {
		switch (ctrl_type) {
		default:
		case BT_8822B_1ANT_EXT_ANT_SWITCH_CTRL_BY_BBSW:
			/*  0x4c[23] = 0 */
			btcoexist->btc_write_1byte_bitmask(btcoexist, 0x4e,
							   0x80, 0x0);
			/* 0x4c[24] = 1 */
			btcoexist->btc_write_1byte_bitmask(btcoexist, 0x4f,
							   0x01, 0x1);
			/* BB SW, DPDT use RFE_ctrl8 and RFE_ctrl9 as ctrl pin*/
			btcoexist->btc_write_1byte_bitmask(btcoexist, 0xcb4,
							   0xff, 0x77);

			/* 0xcbd[1:0] = 2b'01 for no switch_polatiry_inverse,
			 * ANTSWB =1, ANTSW =0
			 */
			regval_0xcbd = (!switch_polatiry_inverse ? 0x1 : 0x2);
			btcoexist->btc_write_1byte_bitmask(btcoexist, 0xcbd,
							   0x3, regval_0xcbd);

			break;
		case BT_8822B_1ANT_EXT_ANT_SWITCH_CTRL_BY_PTA:
			/* 0x4c[23] = 0 */
			btcoexist->btc_write_1byte_bitmask(btcoexist, 0x4e,
							   0x80, 0x0);
			/* 0x4c[24] = 1 */
			btcoexist->btc_write_1byte_bitmask(btcoexist, 0x4f,
							   0x01, 0x1);
			/* PTA,  DPDT use RFE_ctrl8 and RFE_ctrl9 as ctrl pin */
			btcoexist->btc_write_1byte_bitmask(btcoexist, 0xcb4,
							   0xff, 0x66);

			/* 0xcbd[1:0] = 2b'10 for no switch_polatiry_inverse,
			 * ANTSWB =1, ANTSW =0  @ GNT_BT=1
			 */
			regval_0xcbd = (!switch_polatiry_inverse ? 0x2 : 0x1);
			btcoexist->btc_write_1byte_bitmask(btcoexist, 0xcbd,
							   0x3, regval_0xcbd);

			break;
		case BT_8822B_1ANT_EXT_ANT_SWITCH_CTRL_BY_ANTDIV:
			/* 0x4c[23] = 0 */
			btcoexist->btc_write_1byte_bitmask(btcoexist, 0x4e,
							   0x80, 0x0);
			/* 0x4c[24] = 1 */
			btcoexist->btc_write_1byte_bitmask(btcoexist, 0x4f,
							   0x01, 0x1);
			btcoexist->btc_write_1byte_bitmask(btcoexist, 0xcb4,
							   0xff, 0x88);

			/* no regval_0xcbd setup required, because
			 * antenna switch control value by antenna diversity
			 */

			break;
		case BT_8822B_1ANT_EXT_ANT_SWITCH_CTRL_BY_MAC:
			/*  0x4c[23] = 1 */
			btcoexist->btc_write_1byte_bitmask(btcoexist, 0x4e,
							   0x80, 0x1);

			/* 0x64[0] = 1b'0 for no switch_polatiry_inverse,
			 * DPDT_SEL_N =1, DPDT_SEL_P =0
			 */
			regval_0x64 = (!switch_polatiry_inverse ? 0x0 : 0x1);
			btcoexist->btc_write_1byte_bitmask(btcoexist, 0x64, 0x1,
							   regval_0x64);
			break;
		case BT_8822B_1ANT_EXT_ANT_SWITCH_CTRL_BY_BT:
			/* 0x4c[23] = 0 */
			btcoexist->btc_write_1byte_bitmask(btcoexist, 0x4e,
							   0x80, 0x0);
			/* 0x4c[24] = 0 */
			btcoexist->btc_write_1byte_bitmask(btcoexist, 0x4f,
							   0x01, 0x0);

			/* no setup required, because antenna switch control
			 * value by BT vendor 0xac[1:0]
			 */
			break;
		}
	}

	u32tmp1 = btcoexist->btc_read_4byte(btcoexist, 0xcbc);
	u32tmp2 = btcoexist->btc_read_4byte(btcoexist, 0x4c);
	u32tmp3 = btcoexist->btc_read_4byte(btcoexist, 0x64) & 0xff;

	RT_TRACE(
		rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		"[BTCoex], ********** (After Ext Ant switch setup) 0xcbc = 0x%08x, 0x4c = 0x%08x, 0x64= 0x%02x**********\n",
		u32tmp1, u32tmp2, u32tmp3);
}

/* set gnt_wl gnt_bt control by sw high low, or
 * hwpta while in power on, ini, wlan off, wlan only, wl2g non-currrent,
 * wl2g current, wl5g
 */

static void halbtc8822b1ant_set_ant_path(struct btc_coexist *btcoexist,
					 u8 ant_pos_type, bool force_exec,
					 u8 phase)

{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	u8 u8tmp = 0;
	u32 u32tmp1 = 0;
	u32 u32tmp2 = 0, u32tmp3 = 0;

	u32tmp1 = halbtc8822b1ant_ltecoex_indirect_read_reg(btcoexist, 0x38);

	/* To avoid indirect access fail  */
	if (((u32tmp1 & 0xf000) >> 12) != ((u32tmp1 & 0x0f00) >> 8)) {
		force_exec = true;
		coex_sta->gnt_error_cnt++;

		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex],(Before Ant Setup) 0x38= 0x%x\n", u32tmp1);
	}

	/* Ext switch buffer mux */
	btcoexist->btc_write_1byte(btcoexist, 0x974, 0xff);
	btcoexist->btc_write_1byte_bitmask(btcoexist, 0x1991, 0x3, 0x0);
	btcoexist->btc_write_1byte_bitmask(btcoexist, 0xcbe, 0x8, 0x0);

	coex_dm->cur_ant_pos_type = (ant_pos_type << 8) + phase;

	if (!force_exec) {
		if (coex_dm->cur_ant_pos_type == coex_dm->pre_ant_pos_type)
			return;
	}

	coex_dm->pre_ant_pos_type = coex_dm->cur_ant_pos_type;

	if (btcoexist->dbg_mode_1ant) {
		u32tmp1 = halbtc8822b1ant_ltecoex_indirect_read_reg(btcoexist,
								    0x38);
		u32tmp2 = halbtc8822b1ant_ltecoex_indirect_read_reg(btcoexist,
								    0x54);
		u32tmp3 = btcoexist->btc_read_4byte(btcoexist, 0xcb4);

		u8tmp = btcoexist->btc_read_1byte(btcoexist, 0x73);

		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], ********** (Before Ant Setup) 0xcb4 = 0x%x, 0x73 = 0x%x, 0x38= 0x%x, 0x54= 0x%x**********\n",
			u32tmp3, u8tmp, u32tmp1, u32tmp2);
	}

	switch (phase) {
	case BT_8822B_1ANT_PHASE_COEX_INIT:
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], ********** (set_ant_path - 1ANT_PHASE_COEX_INIT) **********\n");

		/* Disable LTE Coex Function in WiFi side
		 * (this should be on if LTE coex is required)
		 */
		halbtc8822b1ant_ltecoex_enable(btcoexist, 0x0);

		/* GNT_WL_LTE always = 1
		 * (this should be config if LTE coex is required)
		 */
		halbtc8822b1ant_ltecoex_set_coex_table(
			btcoexist, BT_8822B_1ANT_CTT_WL_VS_LTE, 0xffff);

		/* GNT_BT_LTE always = 1
		 * (this should be config if LTE coex is required)
		 */
		halbtc8822b1ant_ltecoex_set_coex_table(
			btcoexist, BT_8822B_1ANT_CTT_BT_VS_LTE, 0xffff);

		/* set GNT_BT to SW high */
		halbtc8822b1ant_ltecoex_set_gnt_bt(
			btcoexist, BT_8822B_1ANT_GNT_BLOCK_RFC_BB,
			BT_8822B_1ANT_GNT_CTRL_BY_SW,
			BT_8822B_1ANT_SIG_STA_SET_TO_HIGH);

		/* set GNT_WL to SW low */
		halbtc8822b1ant_ltecoex_set_gnt_wl(
			btcoexist, BT_8822B_1ANT_GNT_BLOCK_RFC_BB,
			BT_8822B_1ANT_GNT_CTRL_BY_SW,
			BT_8822B_1ANT_SIG_STA_SET_TO_LOW);

		/* set Path control owner to WL at initial step */
		halbtc8822b1ant_ltecoex_pathcontrol_owner(
			btcoexist, BT_8822B_1ANT_PCO_WLSIDE);

		coex_sta->run_time_state = false;

		/* Ext switch buffer mux */
		btcoexist->btc_write_1byte(btcoexist, 0x974, 0xff);
		btcoexist->btc_write_1byte_bitmask(btcoexist, 0x1991, 0x3, 0x0);
		btcoexist->btc_write_1byte_bitmask(btcoexist, 0xcbe, 0x8, 0x0);

		if (ant_pos_type == BTC_ANT_PATH_AUTO)
			ant_pos_type = BTC_ANT_PATH_BT;

		break;
	case BT_8822B_1ANT_PHASE_WLANONLY_INIT:
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], ********** (set_ant_path - 1ANT_PHASE_WLANONLY_INIT) **********\n");

		/* Disable LTE Coex Function in WiFi side
		 * (this should be on if LTE coex is required)
		 */
		halbtc8822b1ant_ltecoex_enable(btcoexist, 0x0);

		/* GNT_WL_LTE always = 1
		 * (this should be config if LTE coex is required)
		 */
		halbtc8822b1ant_ltecoex_set_coex_table(
			btcoexist, BT_8822B_1ANT_CTT_WL_VS_LTE, 0xffff);

		/* GNT_BT_LTE always = 1
		 * (this should be config if LTE coex is required)
		 */
		halbtc8822b1ant_ltecoex_set_coex_table(
			btcoexist, BT_8822B_1ANT_CTT_BT_VS_LTE, 0xffff);

		/* set GNT_BT to SW Low */
		halbtc8822b1ant_ltecoex_set_gnt_bt(
			btcoexist, BT_8822B_1ANT_GNT_BLOCK_RFC_BB,
			BT_8822B_1ANT_GNT_CTRL_BY_SW,
			BT_8822B_1ANT_SIG_STA_SET_TO_LOW);

		/* Set GNT_WL to SW high */
		halbtc8822b1ant_ltecoex_set_gnt_wl(
			btcoexist, BT_8822B_1ANT_GNT_BLOCK_RFC_BB,
			BT_8822B_1ANT_GNT_CTRL_BY_SW,
			BT_8822B_1ANT_SIG_STA_SET_TO_HIGH);

		/* set Path control owner to WL at initial step */
		halbtc8822b1ant_ltecoex_pathcontrol_owner(
			btcoexist, BT_8822B_1ANT_PCO_WLSIDE);

		coex_sta->run_time_state = false;

		/* Ext switch buffer mux */
		btcoexist->btc_write_1byte(btcoexist, 0x974, 0xff);
		btcoexist->btc_write_1byte_bitmask(btcoexist, 0x1991, 0x3, 0x0);
		btcoexist->btc_write_1byte_bitmask(btcoexist, 0xcbe, 0x8, 0x0);

		if (ant_pos_type == BTC_ANT_PATH_AUTO)
			ant_pos_type = BTC_ANT_PATH_WIFI;

		break;
	case BT_8822B_1ANT_PHASE_WLAN_OFF:
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], ********** (set_ant_path - 1ANT_PHASE_WLAN_OFF) **********\n");

		/* Disable LTE Coex Function in WiFi side */
		halbtc8822b1ant_ltecoex_enable(btcoexist, 0x0);

		/* set Path control owner to BT */
		halbtc8822b1ant_ltecoex_pathcontrol_owner(
			btcoexist, BT_8822B_1ANT_PCO_BTSIDE);

		/* Set Ext Ant Switch to BT control at wifi off step */
		halbtc8822b1ant_set_ext_ant_switch(
			btcoexist, FORCE_EXEC,
			BT_8822B_1ANT_EXT_ANT_SWITCH_CTRL_BY_BT,
			BT_8822B_1ANT_EXT_ANT_SWITCH_TO_NOCARE);

		coex_sta->run_time_state = false;

		break;
	case BT_8822B_1ANT_PHASE_2G_RUNTIME:
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], ********** (set_ant_path - 1ANT_PHASE_2G_RUNTIME) **********\n");

		/* set GNT_BT to PTA */
		halbtc8822b1ant_ltecoex_set_gnt_bt(
			btcoexist, BT_8822B_1ANT_GNT_BLOCK_RFC_BB,
			BT_8822B_1ANT_GNT_CTRL_BY_PTA,
			BT_8822B_1ANT_SIG_STA_SET_BY_HW);

		/* Set GNT_WL to PTA */
		halbtc8822b1ant_ltecoex_set_gnt_wl(
			btcoexist, BT_8822B_1ANT_GNT_BLOCK_RFC_BB,
			BT_8822B_1ANT_GNT_CTRL_BY_PTA,
			BT_8822B_1ANT_SIG_STA_SET_BY_HW);

		/* set Path control owner to WL at runtime step */
		halbtc8822b1ant_ltecoex_pathcontrol_owner(
			btcoexist, BT_8822B_1ANT_PCO_WLSIDE);

		coex_sta->run_time_state = true;

		if (ant_pos_type == BTC_ANT_PATH_AUTO)
			ant_pos_type = BTC_ANT_PATH_PTA;

		break;
	case BT_8822B_1ANT_PHASE_5G_RUNTIME:
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], ********** (set_ant_path - 1ANT_PHASE_5G_RUNTIME) **********\n");

		/* set GNT_BT to SW Hi */
		halbtc8822b1ant_ltecoex_set_gnt_bt(
			btcoexist, BT_8822B_1ANT_GNT_BLOCK_RFC_BB,
			BT_8822B_1ANT_GNT_CTRL_BY_SW,
			BT_8822B_1ANT_SIG_STA_SET_TO_HIGH);

		/* Set GNT_WL to SW Hi */
		halbtc8822b1ant_ltecoex_set_gnt_wl(
			btcoexist, BT_8822B_1ANT_GNT_BLOCK_RFC_BB,
			BT_8822B_1ANT_GNT_CTRL_BY_SW,
			BT_8822B_1ANT_SIG_STA_SET_TO_HIGH);

		/* set Path control owner to WL at runtime step */
		halbtc8822b1ant_ltecoex_pathcontrol_owner(
			btcoexist, BT_8822B_1ANT_PCO_WLSIDE);

		coex_sta->run_time_state = true;

		if (ant_pos_type == BTC_ANT_PATH_AUTO)
			ant_pos_type = BTC_ANT_PATH_WIFI5G;

		break;
	case BT_8822B_1ANT_PHASE_BTMPMODE:
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], ********** (set_ant_path - 1ANT_PHASE_BTMPMODE) **********\n");

		/* Disable LTE Coex Function in WiFi side */
		halbtc8822b1ant_ltecoex_enable(btcoexist, 0x0);

		/* set GNT_BT to SW Hi */
		halbtc8822b1ant_ltecoex_set_gnt_bt(
			btcoexist, BT_8822B_1ANT_GNT_BLOCK_RFC_BB,
			BT_8822B_1ANT_GNT_CTRL_BY_SW,
			BT_8822B_1ANT_SIG_STA_SET_TO_HIGH);

		/* Set GNT_WL to SW Lo */
		halbtc8822b1ant_ltecoex_set_gnt_wl(
			btcoexist, BT_8822B_1ANT_GNT_BLOCK_RFC_BB,
			BT_8822B_1ANT_GNT_CTRL_BY_SW,
			BT_8822B_1ANT_SIG_STA_SET_TO_LOW);

		/* set Path control owner to WL */
		halbtc8822b1ant_ltecoex_pathcontrol_owner(
			btcoexist, BT_8822B_1ANT_PCO_WLSIDE);

		coex_sta->run_time_state = false;

		/* Set Ext Ant Switch to BT side at BT MP mode */
		if (ant_pos_type == BTC_ANT_PATH_AUTO)
			ant_pos_type = BTC_ANT_PATH_BT;

		break;
	}

	if (phase != BT_8822B_1ANT_PHASE_WLAN_OFF) {
		switch (ant_pos_type) {
		case BTC_ANT_PATH_WIFI:
			halbtc8822b1ant_set_ext_ant_switch(
				btcoexist, force_exec,
				BT_8822B_1ANT_EXT_ANT_SWITCH_CTRL_BY_BBSW,
				BT_8822B_1ANT_EXT_ANT_SWITCH_TO_WLG);
			break;
		case BTC_ANT_PATH_WIFI5G:
			halbtc8822b1ant_set_ext_ant_switch(
				btcoexist, force_exec,
				BT_8822B_1ANT_EXT_ANT_SWITCH_CTRL_BY_BBSW,
				BT_8822B_1ANT_EXT_ANT_SWITCH_TO_WLA);
			break;
		case BTC_ANT_PATH_BT:
			halbtc8822b1ant_set_ext_ant_switch(
				btcoexist, force_exec,
				BT_8822B_1ANT_EXT_ANT_SWITCH_CTRL_BY_BBSW,
				BT_8822B_1ANT_EXT_ANT_SWITCH_TO_BT);
			break;
		default:
		case BTC_ANT_PATH_PTA:
			halbtc8822b1ant_set_ext_ant_switch(
				btcoexist, force_exec,
				BT_8822B_1ANT_EXT_ANT_SWITCH_CTRL_BY_PTA,
				BT_8822B_1ANT_EXT_ANT_SWITCH_TO_NOCARE);
			break;
		}
	}

	if (btcoexist->dbg_mode_1ant) {
		u32tmp1 = halbtc8822b1ant_ltecoex_indirect_read_reg(btcoexist,
								    0x38);
		u32tmp2 = halbtc8822b1ant_ltecoex_indirect_read_reg(btcoexist,
								    0x54);
		u32tmp3 = btcoexist->btc_read_4byte(btcoexist, 0xcb4);

		u8tmp = btcoexist->btc_read_1byte(btcoexist, 0x73);

		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], ********** (After Ant Setup) 0xcb4 = 0x%x, 0x73 = 0x%x, 0x38= 0x%x, 0x54= 0x%x**********\n",
			u32tmp3, u8tmp, u32tmp1, u32tmp2);
	}
}

static bool halbtc8822b1ant_is_common_action(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	bool common = false, wifi_connected = false, wifi_busy = false;

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_CONNECTED,
			   &wifi_connected);
	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_BUSY, &wifi_busy);

	if (!wifi_connected &&
	    coex_dm->bt_status == BT_8822B_1ANT_BT_STATUS_NON_CONNECTED_IDLE) {
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], Wifi non connected-idle + BT non connected-idle!!\n");

		/* halbtc8822b1ant_sw_mechanism(btcoexist, false); */

		common = true;
	} else if (wifi_connected &&
		   (coex_dm->bt_status ==
		    BT_8822B_1ANT_BT_STATUS_NON_CONNECTED_IDLE)) {
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], Wifi connected + BT non connected-idle!!\n");

		/* halbtc8822b1ant_sw_mechanism(btcoexist, false); */

		common = true;
	} else if (!wifi_connected && (BT_8822B_1ANT_BT_STATUS_CONNECTED_IDLE ==
				       coex_dm->bt_status)) {
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], Wifi non connected-idle + BT connected-idle!!\n");

		/* halbtc8822b1ant_sw_mechanism(btcoexist, false); */

		common = true;
	} else if (wifi_connected && (BT_8822B_1ANT_BT_STATUS_CONNECTED_IDLE ==
				      coex_dm->bt_status)) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], Wifi connected + BT connected-idle!!\n");

		/* halbtc8822b1ant_sw_mechanism(btcoexist, false); */

		common = true;
	} else if (!wifi_connected && (BT_8822B_1ANT_BT_STATUS_CONNECTED_IDLE !=
				       coex_dm->bt_status)) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], Wifi non connected-idle + BT Busy!!\n");

		/* halbtc8822b1ant_sw_mechanism(btcoexist, false); */

		common = true;
	} else {
		if (wifi_busy) {
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "[BTCoex], Wifi Connected-Busy + BT Busy!!\n");
		} else {
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "[BTCoex], Wifi Connected-Idle + BT Busy!!\n");
		}

		common = false;
	}

	return common;
}

static void halbtc8822b1ant_action_wifi_under5g(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex], under 5g start\n");
	/* for test : s3 bt disappear , fail rate 1/600*/
	/*set sw gnt wl bt  high*/
	halbtc8822b1ant_set_ant_path(btcoexist, BTC_ANT_PATH_AUTO, FORCE_EXEC,
				     BT_8822B_1ANT_PHASE_5G_RUNTIME);

	halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, false, 8);
	halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 0);
	btcoexist->btc_write_1byte_bitmask(btcoexist, 0xcbd, 0x3, 1);
}

static void halbtc8822b1ant_action_wifi_only(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	bool wifi_under_5g = false, rf4ce_enabled = false;

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_UNDER_5G, &wifi_under_5g);
	if (wifi_under_5g) {
		halbtc8822b1ant_action_wifi_under5g(btcoexist);

		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], ********** (wlan only -- under 5g ) **********\n");
		return;
	}

	if (rf4ce_enabled) {
		btcoexist->btc_write_1byte_bitmask(btcoexist, 0x45e, 0x8, 0x1);

		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true, 50);

		halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 1);
		return;
	}
	halbtc8822b1ant_coex_table_with_type(btcoexist, FORCE_EXEC, 0);
	halbtc8822b1ant_ps_tdma(btcoexist, FORCE_EXEC, false, 8);

	halbtc8822b1ant_set_ant_path(btcoexist, BTC_ANT_PATH_AUTO, FORCE_EXEC,
				     BT_8822B_1ANT_PHASE_2G_RUNTIME);

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex], ********** (wlan only -- under 2g ) **********\n");
}

static void
halbtc8822b1ant_action_wifi_native_lps(struct btc_coexist *btcoexist)
{
	halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 5);

	halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, false, 8);
}

/* *********************************************
 *
 *	Non-Software Coex Mechanism start
 *
 * **********************************************/

static void halbtc8822b1ant_action_bt_whck_test(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex],action_bt_whck_test\n");

	halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, false, 8);

	halbtc8822b1ant_set_ant_path(btcoexist, BTC_ANT_PATH_AUTO, NORMAL_EXEC,
				     BT_8822B_1ANT_PHASE_2G_RUNTIME);

	halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 0);
}

static void
halbtc8822b1ant_action_wifi_multi_port(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex],action_wifi_multi_port\n");

	halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, false, 8);

	halbtc8822b1ant_set_ant_path(btcoexist, BTC_ANT_PATH_AUTO, NORMAL_EXEC,
				     BT_8822B_1ANT_PHASE_2G_RUNTIME);

	halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 0);
}

static void halbtc8822b1ant_action_hs(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD, "[BTCoex], action_hs\n");

	halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true, 5);

	halbtc8822b1ant_set_ant_path(btcoexist, BTC_ANT_PATH_AUTO, NORMAL_EXEC,
				     BT_8822B_1ANT_PHASE_2G_RUNTIME);

	halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 5);
}

static void halbtc8822b1ant_action_bt_relink(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex], run bt multi link function\n");

	if (coex_sta->is_bt_multi_link)
		return;
	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex], run bt_re-link function\n");

	halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, false, 8);
	halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 0);
}

/*"""bt inquiry"""" + wifi any + bt any*/

static void halbtc8822b1ant_action_bt_inquiry(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	struct btc_bt_link_info *bt_link_info = &btcoexist->bt_link_info;
	bool wifi_connected = false, ap_enable = false, wifi_busy = false,
	     bt_busy = false, rf4ce_enabled = false;

	bool wifi_scan = false, link = false, roam = false;

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex], ********** (bt inquiry) **********\n");
	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_AP_MODE_ENABLE,
			   &ap_enable);
	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_CONNECTED,
			   &wifi_connected);
	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_BUSY, &wifi_busy);
	btcoexist->btc_set(btcoexist, BTC_SET_BL_BT_TRAFFIC_BUSY, &bt_busy);

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_SCAN, &wifi_scan);
	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_LINK, &link);
	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_ROAM, &roam);

	RT_TRACE(
		rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		"[BTCoex], ********** scan = %d,  link =%d, roam = %d**********\n",
		wifi_scan, link, roam);

	if ((link) || (roam) || (coex_sta->wifi_is_high_pri_task)) {
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], ********** (bt inquiry wifi  connect or scan ) **********\n");

		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true, 1);

		halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 6);

	} else if ((wifi_scan) && (coex_sta->bt_create_connection)) {
		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true, 22);
		halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 6);

	} else if ((!wifi_connected) && (!wifi_scan)) {
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], ********** (bt inquiry wifi non connect) **********\n");

		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, false, 8);

		halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 0);

	} else if ((bt_link_info->a2dp_exist) && (bt_link_info->pan_exist)) {
		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true, 22);
		halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 4);
	} else if (bt_link_info->a2dp_exist) {
		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true, 32);

		halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 3);
	} else if (wifi_scan) {
		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true, 20);

		halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 4);
	} else if (wifi_busy) {
		/* for BT inquiry/page fail after S4 resume */
		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true, 32);
		/*aaaa->55aa for bt connect while wl busy*/
		halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC,
						     15);
		if (rf4ce_enabled) {
			btcoexist->btc_write_1byte_bitmask(btcoexist, 0x45e,
							   0x8, 0x1);

			halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true,
						50);

			halbtc8822b1ant_coex_table_with_type(btcoexist,
							     NORMAL_EXEC, 0);
		}
	} else {
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], ********** (bt inquiry wifi connect) **********\n");

		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true, 22);

		halbtc8822b1ant_set_ant_path(btcoexist, BTC_ANT_PATH_AUTO,
					     NORMAL_EXEC,
					     BT_8822B_1ANT_PHASE_2G_RUNTIME);

		halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 4);
	}
}

static void
halbtc8822b1ant_action_bt_sco_hid_only_busy(struct btc_coexist *btcoexist)
{
	struct btc_bt_link_info *bt_link_info = &btcoexist->bt_link_info;
	bool wifi_connected = false, wifi_busy = false;
	u32 wifi_bw = 1;

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_CONNECTED,
			   &wifi_connected);

	btcoexist->btc_get(btcoexist, BTC_GET_U4_WIFI_BW, &wifi_bw);

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_BUSY, &wifi_busy);

	if (bt_link_info->sco_exist) {
		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true, 5);
		halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 5);
	} else {
		if (coex_sta->is_hid_low_pri_tx_overhead) {
			halbtc8822b1ant_coex_table_with_type(btcoexist,
							     NORMAL_EXEC, 6);
			halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true,
						18);
		} else if (wifi_bw == 0) { /* if 11bg mode */

			if (coex_sta->is_bt_multi_link) {
				halbtc8822b1ant_coex_table_with_type(
					btcoexist, NORMAL_EXEC, 11);
				halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC,
							true, 11);
			} else {
				halbtc8822b1ant_coex_table_with_type(
					btcoexist, NORMAL_EXEC, 6);
				halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC,
							true, 11);
			}
		} else {
			halbtc8822b1ant_coex_table_with_type(btcoexist,
							     NORMAL_EXEC, 6);
			halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true,
						11);
		}
	}
}

static void
halbtc8822b1ant_action_wifi_connected_bt_acl_busy(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	struct btc_bt_link_info *bt_link_info = &btcoexist->bt_link_info;
	bool wifi_busy = false, wifi_turbo = false;
	u32 wifi_bw = 1;

	btcoexist->btc_get(btcoexist, BTC_GET_U4_WIFI_BW, &wifi_bw);

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_BUSY, &wifi_busy);
	btcoexist->btc_get(btcoexist, BTC_GET_U1_AP_NUM,
			   &coex_sta->scan_ap_num);
	RT_TRACE(
		rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		"############# [BTCoex],  scan_ap_num = %d, wl_noisy_level = %d\n",
		coex_sta->scan_ap_num, coex_sta->wl_noisy_level);

	if ((wifi_busy) && (coex_sta->wl_noisy_level == 0))
		wifi_turbo = true;

	if ((coex_sta->bt_relink_downcount != 0) &&
	    (!bt_link_info->pan_exist) && (wifi_busy)) {
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"############# [BTCoex],  BT Re-Link + A2DP + WL busy\n");

		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, false, 8);
		halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 0);

	} else if ((bt_link_info->a2dp_exist) && (coex_sta->is_bt_a2dp_sink)) {
		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true, 12);
		halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 6);
	} else if (bt_link_info->a2dp_only) { /* A2DP		 */

		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true, 7);

		if (wifi_turbo)
			halbtc8822b1ant_coex_table_with_type(btcoexist,
							     NORMAL_EXEC, 19);
		else
			halbtc8822b1ant_coex_table_with_type(btcoexist,
							     NORMAL_EXEC, 4);
	} else if (((bt_link_info->a2dp_exist) && (bt_link_info->pan_exist)) ||
		   (bt_link_info->hid_exist && bt_link_info->a2dp_exist &&
		    bt_link_info->pan_exist)) {
		/* A2DP+PAN(OPP,FTP), HID+A2DP+PAN(OPP,FTP) */

		if (wifi_busy)
			halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true,
						13);
		else
			halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true,
						14);

		if (bt_link_info->hid_exist)
			halbtc8822b1ant_coex_table_with_type(btcoexist,
							     NORMAL_EXEC, 1);
		else if (wifi_turbo)
			halbtc8822b1ant_coex_table_with_type(btcoexist,
							     NORMAL_EXEC, 19);
		else
			halbtc8822b1ant_coex_table_with_type(btcoexist,
							     NORMAL_EXEC, 4);
	} else if (bt_link_info->hid_exist &&
		   bt_link_info->a2dp_exist) { /* HID+A2DP */

		if (wifi_bw == 0) { /* if 11bg mode */
			halbtc8822b1ant_coex_table_with_type(btcoexist,
							     NORMAL_EXEC, 1);
			halbtc8822b1ant_set_wltoggle_coex_table(
				btcoexist, NORMAL_EXEC, 1, 0xaa, 0x5a, 0xaa,
				0xaa);
			halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true,
						49);
		} else {
			halbtc8822b1ant_coex_table_with_type(btcoexist,
							     NORMAL_EXEC, 1);
			halbtc8822b1ant_limited_rx(btcoexist, NORMAL_EXEC,
						   false, true, 8);
			halbtc8822b1ant_set_wltoggle_coex_table(
				btcoexist, NORMAL_EXEC, 1, 0xaa, 0x5a, 0xaa,
				0xaa);
			halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true,
						49);
		}
		/* PAN(OPP,FTP), HID+PAN(OPP,FTP) */

	} else if ((bt_link_info->pan_only) ||
		   (bt_link_info->hid_exist && bt_link_info->pan_exist)) {
		if (!wifi_busy)
			halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true,
						4);
		else
			halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true,
						3);

		if (bt_link_info->hid_exist)
			halbtc8822b1ant_coex_table_with_type(btcoexist,
							     NORMAL_EXEC, 1);
		else if (wifi_turbo)
			halbtc8822b1ant_coex_table_with_type(btcoexist,
							     NORMAL_EXEC, 19);
		else
			halbtc8822b1ant_coex_table_with_type(btcoexist,
							     NORMAL_EXEC, 4);
	} else {
		/* BT no-profile busy (0x9) */
		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true, 33);
		halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 4);
	}
}

/*wifi not connected + bt action*/

static void
halbtc8822b1ant_action_wifi_not_connected(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	bool rf4ce_enabled = false;

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex], ********** (wifi not connect) **********\n");

	/* tdma and coex table */
	if (rf4ce_enabled) {
		btcoexist->btc_write_1byte_bitmask(btcoexist, 0x45e, 0x8, 0x1);

		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true, 50);

		halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 1);
		return;
	}
	halbtc8822b1ant_ps_tdma(btcoexist, FORCE_EXEC, false, 8);

	halbtc8822b1ant_set_ant_path(btcoexist, BTC_ANT_PATH_AUTO, NORMAL_EXEC,
				     BT_8822B_1ANT_PHASE_2G_RUNTIME);

	halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 0);
}

/*""""wl not connected scan"""" + bt action*/
static void
halbtc8822b1ant_action_wifi_not_connected_scan(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	struct btc_bt_link_info *bt_link_info = &btcoexist->bt_link_info;
	bool bt_hs_on = false;
	u32 wifi_link_status = 0;
	u32 num_of_wifi_link = 0;
	bool bt_ctrl_agg_buf_size = false;
	u8 agg_buf_size = 5;

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex], ********** (wifi non connect scan) **********\n");

	btcoexist->btc_get(btcoexist, BTC_GET_BL_HS_OPERATION, &bt_hs_on);
	btcoexist->btc_get(btcoexist, BTC_GET_U4_WIFI_LINK_STATUS,
			   &wifi_link_status);

	num_of_wifi_link = wifi_link_status >> 16;

	if (num_of_wifi_link >= 2) {
		halbtc8822b1ant_limited_tx(btcoexist, NORMAL_EXEC, 0, 0, 0, 0);
		halbtc8822b1ant_limited_rx(btcoexist, NORMAL_EXEC, false,
					   bt_ctrl_agg_buf_size, agg_buf_size);

		if (coex_sta->c2h_bt_inquiry_page) {
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "############# [BTCoex],  BT Is Inquirying\n");
			halbtc8822b1ant_action_bt_inquiry(btcoexist);
		} else {
			halbtc8822b1ant_action_wifi_multi_port(btcoexist);
		}
		return;
	}

	if (coex_sta->c2h_bt_inquiry_page) {
		halbtc8822b1ant_action_bt_inquiry(btcoexist);
		return;
	} else if (bt_hs_on) {
		halbtc8822b1ant_action_hs(btcoexist);
		return;
	}

	/* tdma and coex table */
	if (coex_dm->bt_status == BT_8822B_1ANT_BT_STATUS_ACL_BUSY) {
		if (bt_link_info->a2dp_exist) {
			halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true,
						32);
			halbtc8822b1ant_coex_table_with_type(btcoexist,
							     NORMAL_EXEC, 1);
		} else if (bt_link_info->a2dp_exist &&
			   bt_link_info->pan_exist) {
			halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true,
						22);
			halbtc8822b1ant_coex_table_with_type(btcoexist,
							     NORMAL_EXEC, 1);
		} else {
			halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true,
						20);
			halbtc8822b1ant_coex_table_with_type(btcoexist,
							     NORMAL_EXEC, 1);
		}
	} else if ((coex_dm->bt_status == BT_8822B_1ANT_BT_STATUS_SCO_BUSY) ||
		   (BT_8822B_1ANT_BT_STATUS_ACL_SCO_BUSY ==
		    coex_dm->bt_status)) {
		halbtc8822b1ant_action_bt_sco_hid_only_busy(btcoexist);
	} else {
		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, false, 8);

		halbtc8822b1ant_set_ant_path(btcoexist, BTC_ANT_PATH_AUTO,
					     NORMAL_EXEC,
					     BT_8822B_1ANT_PHASE_2G_RUNTIME);

		halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 5);
	}
}

/*""""wl not connected asso"""" + bt action*/

static void halbtc8822b1ant_action_wifi_not_connected_asso_auth(
	struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	struct btc_bt_link_info *bt_link_info = &btcoexist->bt_link_info;
	bool bt_hs_on = false;
	u32 wifi_link_status = 0;
	u32 num_of_wifi_link = 0;
	bool bt_ctrl_agg_buf_size = false;
	u8 agg_buf_size = 5;

	RT_TRACE(
		rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		"[BTCoex], ********** (wifi non connect asso_auth) **********\n");

	btcoexist->btc_get(btcoexist, BTC_GET_BL_HS_OPERATION, &bt_hs_on);
	btcoexist->btc_get(btcoexist, BTC_GET_U4_WIFI_LINK_STATUS,
			   &wifi_link_status);

	num_of_wifi_link = wifi_link_status >> 16;

	if (num_of_wifi_link >= 2) {
		halbtc8822b1ant_limited_tx(btcoexist, NORMAL_EXEC, 0, 0, 0, 0);
		halbtc8822b1ant_limited_rx(btcoexist, NORMAL_EXEC, false,
					   bt_ctrl_agg_buf_size, agg_buf_size);

		if (coex_sta->c2h_bt_inquiry_page) {
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "############# [BTCoex],  BT Is Inquirying\n");
			halbtc8822b1ant_action_bt_inquiry(btcoexist);
		} else {
			halbtc8822b1ant_action_wifi_multi_port(btcoexist);
		}
		return;
	}

	if (coex_sta->c2h_bt_inquiry_page) {
		halbtc8822b1ant_action_bt_inquiry(btcoexist);
		return;
	} else if (bt_hs_on) {
		halbtc8822b1ant_action_hs(btcoexist);
		return;
	}

	/* tdma and coex table */
	if ((bt_link_info->sco_exist) || (bt_link_info->hid_exist) ||
	    (bt_link_info->a2dp_exist)) {
		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true, 32);
		halbtc8822b1ant_coex_table_with_type(btcoexist, FORCE_EXEC, 4);
	} else if (bt_link_info->pan_exist) {
		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true, 20);
		halbtc8822b1ant_coex_table_with_type(btcoexist, FORCE_EXEC, 4);
	} else {
		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, false, 8);

		halbtc8822b1ant_set_ant_path(btcoexist, BTC_ANT_PATH_AUTO,
					     NORMAL_EXEC,
					     BT_8822B_1ANT_PHASE_2G_RUNTIME);

		halbtc8822b1ant_coex_table_with_type(btcoexist, FORCE_EXEC, 2);
	}
}

/*""""wl  connected scan"""" + bt action*/

static void
halbtc8822b1ant_action_wifi_connected_scan(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	struct btc_bt_link_info *bt_link_info = &btcoexist->bt_link_info;
	bool bt_hs_on = false;
	u32 wifi_link_status = 0;
	u32 num_of_wifi_link = 0;
	bool bt_ctrl_agg_buf_size = false;
	u8 agg_buf_size = 5;

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex], ********** (wifi connect scan) **********\n");

	btcoexist->btc_get(btcoexist, BTC_GET_BL_HS_OPERATION, &bt_hs_on);
	btcoexist->btc_get(btcoexist, BTC_GET_U4_WIFI_LINK_STATUS,
			   &wifi_link_status);

	num_of_wifi_link = wifi_link_status >> 16;

	if (num_of_wifi_link >= 2) {
		halbtc8822b1ant_limited_tx(btcoexist, NORMAL_EXEC, 0, 0, 0, 0);
		halbtc8822b1ant_limited_rx(btcoexist, NORMAL_EXEC, false,
					   bt_ctrl_agg_buf_size, agg_buf_size);

		if (coex_sta->c2h_bt_inquiry_page) {
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "############# [BTCoex],  BT Is Inquirying\n");
			halbtc8822b1ant_action_bt_inquiry(btcoexist);
		} else {
			halbtc8822b1ant_action_wifi_multi_port(btcoexist);
		}
		return;
	}

	if (coex_sta->c2h_bt_inquiry_page) {
		halbtc8822b1ant_action_bt_inquiry(btcoexist);
		return;
	} else if (bt_hs_on) {
		halbtc8822b1ant_action_hs(btcoexist);
		return;
	}

	/* tdma and coex table */
	if (coex_dm->bt_status == BT_8822B_1ANT_BT_STATUS_ACL_BUSY) {
		if (bt_link_info->a2dp_exist) {
			halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true,
						32);
			halbtc8822b1ant_coex_table_with_type(btcoexist,
							     NORMAL_EXEC, 1);
		} else if (bt_link_info->a2dp_exist &&
			   bt_link_info->pan_exist) {
			halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true,
						22);
			halbtc8822b1ant_coex_table_with_type(btcoexist,
							     NORMAL_EXEC, 1);
		} else {
			halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true,
						20);
			halbtc8822b1ant_coex_table_with_type(btcoexist,
							     NORMAL_EXEC, 1);
		}
	} else if ((coex_dm->bt_status == BT_8822B_1ANT_BT_STATUS_SCO_BUSY) ||
		   (BT_8822B_1ANT_BT_STATUS_ACL_SCO_BUSY ==
		    coex_dm->bt_status)) {
		halbtc8822b1ant_action_bt_sco_hid_only_busy(btcoexist);
	} else {
		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, false, 8);

		halbtc8822b1ant_set_ant_path(btcoexist, BTC_ANT_PATH_AUTO,
					     NORMAL_EXEC,
					     BT_8822B_1ANT_PHASE_2G_RUNTIME);

		halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 6);
	}
}

/*""""wl  connected specific packet"""" + bt action*/

static void halbtc8822b1ant_action_wifi_connected_specific_packet(
	struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	struct btc_bt_link_info *bt_link_info = &btcoexist->bt_link_info;
	bool bt_hs_on = false;
	u32 wifi_link_status = 0;
	u32 num_of_wifi_link = 0;
	bool bt_ctrl_agg_buf_size = false;
	u8 agg_buf_size = 5;
	bool wifi_busy = false;

	RT_TRACE(
		rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		"[BTCoex], ********** (wifi connect specific packet) **********\n");

	btcoexist->btc_get(btcoexist, BTC_GET_BL_HS_OPERATION, &bt_hs_on);
	btcoexist->btc_get(btcoexist, BTC_GET_U4_WIFI_LINK_STATUS,
			   &wifi_link_status);

	num_of_wifi_link = wifi_link_status >> 16;

	if (num_of_wifi_link >= 2) {
		halbtc8822b1ant_limited_tx(btcoexist, NORMAL_EXEC, 0, 0, 0, 0);
		halbtc8822b1ant_limited_rx(btcoexist, NORMAL_EXEC, false,
					   bt_ctrl_agg_buf_size, agg_buf_size);

		if (coex_sta->c2h_bt_inquiry_page) {
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "############# [BTCoex],  BT Is Inquirying\n");
			halbtc8822b1ant_action_bt_inquiry(btcoexist);
		} else {
			halbtc8822b1ant_action_wifi_multi_port(btcoexist);
		}
		return;
	}

	if (coex_sta->c2h_bt_inquiry_page) {
		halbtc8822b1ant_action_bt_inquiry(btcoexist);
		return;
	} else if (bt_hs_on) {
		halbtc8822b1ant_action_hs(btcoexist);
		return;
	}

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_BUSY, &wifi_busy);

	/* no specific packet process for both WiFi and BT very busy */
	if ((wifi_busy) &&
	    ((bt_link_info->pan_exist) || (coex_sta->num_of_profile >= 2)))
		return;

	/* tdma and coex table */
	if ((bt_link_info->sco_exist) || (bt_link_info->hid_exist)) {
		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true, 32);
		halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 5);
	} else if (bt_link_info->a2dp_exist) {
		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true, 32);
		/*for a2dp glitch,change from 1 to 15*/
		halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC,
						     15);
	} else if (bt_link_info->pan_exist) {
		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, true, 20);
		halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 1);
	} else {
		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, false, 8);

		halbtc8822b1ant_set_ant_path(btcoexist, BTC_ANT_PATH_AUTO,
					     NORMAL_EXEC,
					     BT_8822B_1ANT_PHASE_2G_RUNTIME);

		halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 5);
	}
}

/* wifi connected input point:
 * to set different ps and tdma case (+bt different case)
 */

static void halbtc8822b1ant_action_wifi_connected(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	bool wifi_busy = false, rf4ce_enabled = false;
	bool scan = false, link = false, roam = false;
	bool under_4way = false, ap_enable = false, wifi_under_5g = false;
	u8 wifi_rssi_state;

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex], CoexForWifiConnect()===>\n");

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_UNDER_5G, &wifi_under_5g);

	if (wifi_under_5g) {
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], CoexForWifiConnect(), return for wifi is under 5g<===\n");

		halbtc8822b1ant_action_wifi_under5g(btcoexist);

		return;
	}

	RT_TRACE(
		rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		"[BTCoex], CoexForWifiConnect(), return for wifi is under 2g<===\n");

	halbtc8822b1ant_set_ant_path(btcoexist, BTC_ANT_PATH_AUTO, NORMAL_EXEC,
				     BT_8822B_1ANT_PHASE_2G_RUNTIME);

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_4_WAY_PROGRESS,
			   &under_4way);

	if (under_4way) {
		halbtc8822b1ant_action_wifi_connected_specific_packet(
			btcoexist);
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], CoexForWifiConnect(), return for wifi is under 4way<===\n");
		return;
	}

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_SCAN, &scan);
	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_LINK, &link);
	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_ROAM, &roam);
	if (scan || link || roam) {
		if (scan)
			halbtc8822b1ant_action_wifi_connected_scan(btcoexist);
		else
			halbtc8822b1ant_action_wifi_connected_specific_packet(
				btcoexist);
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], CoexForWifiConnect(), return for wifi is under scan<===\n");
		return;
	}

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_AP_MODE_ENABLE,
			   &ap_enable);
	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_BUSY, &wifi_busy);

	/* tdma and coex table */
	if (!wifi_busy) {
		if (coex_dm->bt_status == BT_8822B_1ANT_BT_STATUS_ACL_BUSY) {
			halbtc8822b1ant_action_wifi_connected_bt_acl_busy(
				btcoexist);
		} else if ((BT_8822B_1ANT_BT_STATUS_SCO_BUSY ==
			    coex_dm->bt_status) ||
			   (BT_8822B_1ANT_BT_STATUS_ACL_SCO_BUSY ==
			    coex_dm->bt_status)) {
			halbtc8822b1ant_action_bt_sco_hid_only_busy(btcoexist);
		} else {
			halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, false,
						8);

			halbtc8822b1ant_set_ant_path(
				btcoexist, BTC_ANT_PATH_AUTO, NORMAL_EXEC,
				BT_8822B_1ANT_PHASE_2G_RUNTIME);

			if ((coex_sta->high_priority_tx) +
				    (coex_sta->high_priority_rx) <=
			    60)
				/*sy modify case16 -> case17*/
				halbtc8822b1ant_coex_table_with_type(
					btcoexist, NORMAL_EXEC, 1);
			else
				halbtc8822b1ant_coex_table_with_type(
					btcoexist, NORMAL_EXEC, 1);
		}
	} else {
		if (coex_dm->bt_status == BT_8822B_1ANT_BT_STATUS_ACL_BUSY) {
			halbtc8822b1ant_action_wifi_connected_bt_acl_busy(
				btcoexist);
		} else if ((BT_8822B_1ANT_BT_STATUS_SCO_BUSY ==
			    coex_dm->bt_status) ||
			   (BT_8822B_1ANT_BT_STATUS_ACL_SCO_BUSY ==
			    coex_dm->bt_status)) {
			halbtc8822b1ant_action_bt_sco_hid_only_busy(btcoexist);
		} else {
			if (rf4ce_enabled) {
				btcoexist->btc_write_1byte_bitmask(
					btcoexist, 0x45e, 0x8, 0x1);

				halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC,
							true, 50);

				halbtc8822b1ant_coex_table_with_type(
					btcoexist, NORMAL_EXEC, 1);
				return;
			}

			halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, false,
						8);

			halbtc8822b1ant_set_ant_path(
				btcoexist, BTC_ANT_PATH_AUTO, NORMAL_EXEC,
				BT_8822B_1ANT_PHASE_2G_RUNTIME);

			wifi_rssi_state = halbtc8822b1ant_wifi_rssi_state(
				btcoexist, 1, 2, 25, 0);

			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "[BTCoex], ********** before  **********\n");
			if (BT_8822B_1ANT_BT_STATUS_NON_CONNECTED_IDLE ==
			    coex_dm->bt_status) {
				if (rf4ce_enabled) {
					btcoexist->btc_write_1byte_bitmask(
						btcoexist, 0x45e, 0x8, 0x1);

					halbtc8822b1ant_ps_tdma(btcoexist,
								NORMAL_EXEC,
								true, 50);

					halbtc8822b1ant_coex_table_with_type(
						btcoexist, NORMAL_EXEC, 1);
					return;
				}

				halbtc8822b1ant_coex_table_with_type(
					btcoexist, NORMAL_EXEC, 1);
			} else {
				halbtc8822b1ant_coex_table_with_type(
					btcoexist, NORMAL_EXEC, 1);
			}
		}
	}
}

static void
halbtc8822b1ant_run_sw_coexist_mechanism(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	u8 algorithm = 0;

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex], ********** (run sw coexmech) **********\n");
	algorithm = halbtc8822b1ant_action_algorithm(btcoexist);
	coex_dm->cur_algorithm = algorithm;

	if (halbtc8822b1ant_is_common_action(btcoexist)) {
	} else {
		switch (coex_dm->cur_algorithm) {
		case BT_8822B_1ANT_COEX_ALGO_SCO:
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "[BTCoex], Action algorithm = SCO.\n");
			break;
		case BT_8822B_1ANT_COEX_ALGO_HID:
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "[BTCoex], Action algorithm = HID.\n");
			break;
		case BT_8822B_1ANT_COEX_ALGO_A2DP:
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "[BTCoex], Action algorithm = A2DP.\n");
			break;
		case BT_8822B_1ANT_COEX_ALGO_A2DP_PANHS:
			RT_TRACE(
				rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				"[BTCoex], Action algorithm = A2DP+PAN(HS).\n");
			break;
		case BT_8822B_1ANT_COEX_ALGO_PANEDR:
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "[BTCoex], Action algorithm = PAN(EDR).\n");
			break;
		case BT_8822B_1ANT_COEX_ALGO_PANHS:
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "[BTCoex], Action algorithm = HS mode.\n");
			break;
		case BT_8822B_1ANT_COEX_ALGO_PANEDR_A2DP:
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "[BTCoex], Action algorithm = PAN+A2DP.\n");
			break;
		case BT_8822B_1ANT_COEX_ALGO_PANEDR_HID:
			RT_TRACE(
				rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				"[BTCoex], Action algorithm = PAN(EDR)+HID.\n");
			break;
		case BT_8822B_1ANT_COEX_ALGO_HID_A2DP_PANEDR:
			RT_TRACE(
				rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				"[BTCoex], Action algorithm = HID+A2DP+PAN.\n");
			break;
		case BT_8822B_1ANT_COEX_ALGO_HID_A2DP:
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "[BTCoex], Action algorithm = HID+A2DP.\n");
			break;
		default:
			RT_TRACE(
				rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				"[BTCoex], Action algorithm = coexist All Off!!\n");
			break;
		}
		coex_dm->pre_algorithm = coex_dm->cur_algorithm;
	}
}

static void halbtc8822b1ant_run_coexist_mechanism(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	struct btc_bt_link_info *bt_link_info = &btcoexist->bt_link_info;
	bool wifi_connected = false, bt_hs_on = false;
	bool increase_scan_dev_num = false;
	bool bt_ctrl_agg_buf_size = false;
	bool miracast_plus_bt = false;
	u8 agg_buf_size = 5;
	u32 wifi_link_status = 0;
	u32 num_of_wifi_link = 0, wifi_bw;
	u8 iot_peer = BTC_IOT_PEER_UNKNOWN;
	bool wifi_under_5g = false;

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex], RunCoexistMechanism()===>\n");

	if (btcoexist->manual_control) {
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], RunCoexistMechanism(), return for Manual CTRL <===\n");
		return;
	}

	if (btcoexist->stop_coex_dm) {
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], RunCoexistMechanism(), return for Stop Coex DM <===\n");
		return;
	}

	if (coex_sta->under_ips) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], wifi is under IPS !!!\n");
		return;
	}

	if ((coex_sta->under_lps) &&
	    (coex_dm->bt_status != BT_8822B_1ANT_BT_STATUS_ACL_BUSY)) {
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], RunCoexistMechanism(), wifi is under LPS !!!\n");
		halbtc8822b1ant_action_wifi_native_lps(btcoexist);
		return;
	}

	if (!coex_sta->run_time_state) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], return for run_time_state = false !!!\n");
		return;
	}

	if (coex_sta->freeze_coexrun_by_btinfo) {
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], BtInfoNotify(), return for freeze_coexrun_by_btinfo\n");
		return;
	}

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_UNDER_5G, &wifi_under_5g);
	if (wifi_under_5g) {
		halbtc8822b1ant_action_wifi_under5g(btcoexist);

		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], WiFi is under 5G!!!\n");
		return;
	}

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex], WiFi is under 2G!!!\n");

	halbtc8822b1ant_set_ant_path(btcoexist, BTC_ANT_PATH_AUTO, NORMAL_EXEC,
				     BT_8822B_1ANT_PHASE_2G_RUNTIME);

	if (coex_sta->bt_whck_test) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], BT is under WHCK TEST!!!\n");
		halbtc8822b1ant_action_bt_whck_test(btcoexist);
		return;
	}

	if (coex_sta->bt_disabled) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], BT is disabled !!!\n");
		halbtc8822b1ant_action_wifi_only(btcoexist);
		return;
	}

	if (coex_sta->is_setup_link) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], BT is re-link !!!\n");
		halbtc8822b1ant_action_bt_relink(btcoexist);
		return;
	}

	if ((coex_dm->bt_status == BT_8822B_1ANT_BT_STATUS_ACL_BUSY) ||
	    (coex_dm->bt_status == BT_8822B_1ANT_BT_STATUS_SCO_BUSY) ||
	    (coex_dm->bt_status == BT_8822B_1ANT_BT_STATUS_ACL_SCO_BUSY))
		increase_scan_dev_num = true;

	btcoexist->btc_set(btcoexist, BTC_SET_BL_INC_SCAN_DEV_NUM,
			   &increase_scan_dev_num);
	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_CONNECTED,
			   &wifi_connected);

	btcoexist->btc_get(btcoexist, BTC_GET_U4_WIFI_LINK_STATUS,
			   &wifi_link_status);
	num_of_wifi_link = wifi_link_status >> 16;

	if ((num_of_wifi_link >= 2) ||
	    (wifi_link_status & WIFI_P2P_GO_CONNECTED)) {
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"############# [BTCoex],  Multi-Port num_of_wifi_link = %d, wifi_link_status = 0x%x\n",
			num_of_wifi_link, wifi_link_status);

		if (bt_link_info->bt_link_exist) {
			halbtc8822b1ant_limited_tx(btcoexist, NORMAL_EXEC, 1, 1,
						   0, 1);
			miracast_plus_bt = true;
		} else {
			halbtc8822b1ant_limited_tx(btcoexist, NORMAL_EXEC, 0, 0,
						   0, 0);
			miracast_plus_bt = false;
		}
		btcoexist->btc_set(btcoexist, BTC_SET_BL_MIRACAST_PLUS_BT,
				   &miracast_plus_bt);
		halbtc8822b1ant_limited_rx(btcoexist, NORMAL_EXEC, false,
					   bt_ctrl_agg_buf_size, agg_buf_size);

		if ((bt_link_info->a2dp_exist) &&
		    (coex_sta->c2h_bt_inquiry_page)) {
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "############# [BTCoex],  BT Is Inquirying\n");
			halbtc8822b1ant_action_bt_inquiry(btcoexist);
		} else {
			halbtc8822b1ant_action_wifi_multi_port(btcoexist);
		}

		return;
	}

	miracast_plus_bt = false;
	btcoexist->btc_set(btcoexist, BTC_SET_BL_MIRACAST_PLUS_BT,
			   &miracast_plus_bt);

	btcoexist->btc_get(btcoexist, BTC_GET_U4_WIFI_BW, &wifi_bw);

	if ((bt_link_info->bt_link_exist) && (wifi_connected)) {
		halbtc8822b1ant_limited_tx(btcoexist, NORMAL_EXEC, 1, 1, 0, 1);

		btcoexist->btc_get(btcoexist, BTC_GET_U1_IOT_PEER, &iot_peer);

		if (iot_peer != BTC_IOT_PEER_CISCO) {
			if (bt_link_info->sco_exist)
				halbtc8822b1ant_limited_rx(btcoexist,
							   NORMAL_EXEC, true,
							   false, 0x5);
			else
				halbtc8822b1ant_limited_rx(btcoexist,
							   NORMAL_EXEC, false,
							   false, 0x5);
		} else {
			if (bt_link_info->sco_exist) {
				halbtc8822b1ant_limited_rx(btcoexist,
							   NORMAL_EXEC, true,
							   false, 0x5);
			} else {
				if (wifi_bw == BTC_WIFI_BW_HT40)
					halbtc8822b1ant_limited_rx(
						btcoexist, NORMAL_EXEC, false,
						true, 0x10);
				else
					halbtc8822b1ant_limited_rx(
						btcoexist, NORMAL_EXEC, false,
						true, 0x8);
			}
		}

		halbtc8822b1ant_sw_mechanism(btcoexist, true);
		halbtc8822b1ant_run_sw_coexist_mechanism(
			btcoexist); /* just print debug message */
	} else {
		halbtc8822b1ant_limited_tx(btcoexist, NORMAL_EXEC, 0, 0, 0, 0);

		halbtc8822b1ant_limited_rx(btcoexist, NORMAL_EXEC, false, false,
					   0x5);

		halbtc8822b1ant_sw_mechanism(btcoexist, false);
		halbtc8822b1ant_run_sw_coexist_mechanism(
			btcoexist); /* just print debug message */
	}

	btcoexist->btc_get(btcoexist, BTC_GET_BL_HS_OPERATION, &bt_hs_on);
	if (coex_sta->c2h_bt_inquiry_page) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "############# [BTCoex],  BT Is Inquirying\n");
		halbtc8822b1ant_action_bt_inquiry(btcoexist);
		return;
	} else if (bt_hs_on) {
		halbtc8822b1ant_action_hs(btcoexist);
		return;
	}

	if (!wifi_connected) {
		bool scan = false, link = false, roam = false;

		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], wifi is non connected-idle !!!\n");

		btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_SCAN, &scan);
		btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_LINK, &link);
		btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_ROAM, &roam);

		if (scan)
			halbtc8822b1ant_action_wifi_not_connected_scan(
				btcoexist);
		else if (link || roam)
			halbtc8822b1ant_action_wifi_not_connected_asso_auth(
				btcoexist);
		else
			halbtc8822b1ant_action_wifi_not_connected(btcoexist);
	} else { /* wifi LPS/Busy */
		halbtc8822b1ant_action_wifi_connected(btcoexist);
	}
}

static void halbtc8822b1ant_init_coex_dm(struct btc_coexist *btcoexist)
{
	/* force to reset coex mechanism */

	halbtc8822b1ant_low_penalty_ra(btcoexist, NORMAL_EXEC, false);

	/* sw all off */
	halbtc8822b1ant_sw_mechanism(btcoexist, false);

	coex_sta->pop_event_cnt = 0;
}

static void halbtc8822b1ant_init_hw_config(struct btc_coexist *btcoexist,
					   bool back_up, bool wifi_only)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	u8 u8tmp = 0, i = 0;
	u32 u32tmp1 = 0, u32tmp2 = 0, u32tmp3 = 0;

	u32tmp3 = btcoexist->btc_read_4byte(btcoexist, 0xcb4);
	u32tmp1 = halbtc8822b1ant_ltecoex_indirect_read_reg(btcoexist, 0x38);
	u32tmp2 = halbtc8822b1ant_ltecoex_indirect_read_reg(btcoexist, 0x54);

	RT_TRACE(
		rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		"[BTCoex], ********** (Before Init HW config) 0xcb4 = 0x%x, 0x38= 0x%x, 0x54= 0x%x**********\n",
		u32tmp3, u32tmp1, u32tmp2);

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex], 1Ant Init HW Config!!\n");

	coex_sta->bt_coex_supported_feature = 0;
	coex_sta->bt_coex_supported_version = 0;
	coex_sta->bt_ble_scan_type = 0;
	coex_sta->bt_ble_scan_para[0] = 0;
	coex_sta->bt_ble_scan_para[1] = 0;
	coex_sta->bt_ble_scan_para[2] = 0;
	coex_sta->bt_reg_vendor_ac = 0xffff;
	coex_sta->bt_reg_vendor_ae = 0xffff;
	coex_sta->isolation_btween_wb = BT_8822B_1ANT_DEFAULT_ISOLATION;
	coex_sta->gnt_error_cnt = 0;
	coex_sta->bt_relink_downcount = 0;
	coex_sta->is_set_ps_state_fail = false;
	coex_sta->cnt_set_ps_state_fail = 0;

	for (i = 0; i <= 9; i++)
		coex_sta->bt_afh_map[i] = 0;

	/* Setup RF front end type */
	halbtc8822b1ant_set_rfe_type(btcoexist);

	/* 0xf0[15:12] --> Chip Cut information */
	coex_sta->cut_version =
		(btcoexist->btc_read_1byte(btcoexist, 0xf1) & 0xf0) >> 4;

	btcoexist->btc_write_1byte_bitmask(btcoexist, 0x550, 0x8,
					   0x1); /* enable TBTT nterrupt */

	/* BT report packet sample rate	 */
	/* 0x790[5:0]=0x5 */
	u8tmp = btcoexist->btc_read_1byte(btcoexist, 0x790);
	u8tmp &= 0xc0;
	u8tmp |= 0x5;
	btcoexist->btc_write_1byte(btcoexist, 0x790, u8tmp);

	/* Enable BT counter statistics */
	btcoexist->btc_write_1byte(btcoexist, 0x778, 0x1);

	/* Enable PTA (3-wire function form BT side) */
	btcoexist->btc_write_1byte_bitmask(btcoexist, 0x40, 0x20, 0x1);
	btcoexist->btc_write_1byte_bitmask(btcoexist, 0x41, 0x02, 0x1);

	/* Enable PTA (tx/rx signal form WiFi side) */
	btcoexist->btc_write_1byte_bitmask(btcoexist, 0x4c6, 0x10, 0x1);
	/*GNT_BT=1 while select both */
	btcoexist->btc_write_1byte_bitmask(btcoexist, 0x763, 0x10, 0x1);

	/* enable GNT_WL */
	btcoexist->btc_write_1byte_bitmask(btcoexist, 0x4e, 0x40, 0x0);
	btcoexist->btc_write_1byte_bitmask(btcoexist, 0x67, 0x1, 0x0);

	if (btcoexist->btc_read_1byte(btcoexist, 0x80) == 0xc6)
		halbtc8822b1ant_post_state_to_bt(
			btcoexist, BT_8822B_1ANT_SCOREBOARD_ONOFF, true);

	/* Antenna config */
	if (coex_sta->is_rf_state_off) {
		halbtc8822b1ant_set_ant_path(btcoexist, BTC_ANT_PATH_AUTO,
					     FORCE_EXEC,
					     BT_8822B_1ANT_PHASE_WLAN_OFF);

		btcoexist->stop_coex_dm = true;

		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], **********  %s (RF Off)**********\n",
			 __func__);
	} else if (wifi_only) {
		coex_sta->concurrent_rx_mode_on = false;
		halbtc8822b1ant_set_ant_path(btcoexist, BTC_ANT_PATH_WIFI,
					     FORCE_EXEC,
					     BT_8822B_1ANT_PHASE_WLANONLY_INIT);
	} else {
		coex_sta->concurrent_rx_mode_on = true;

		halbtc8822b1ant_set_ant_path(btcoexist, BTC_ANT_PATH_AUTO,
					     FORCE_EXEC,
					     BT_8822B_1ANT_PHASE_COEX_INIT);
	}

	/* PTA parameter */
	halbtc8822b1ant_coex_table_with_type(btcoexist, FORCE_EXEC, 0);

	halbtc8822b1ant_enable_gnt_to_gpio(btcoexist, true);
}

void ex_btc8822b1ant_power_on_setting(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	struct btc_board_info *board_info = &btcoexist->board_info;
	u8 u8tmp = 0x0;
	u16 u16tmp = 0x0;

	RT_TRACE(
		rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		"xxxxxxxxxxxxxxxx Execute 8822b 1-Ant PowerOn Setting!! xxxxxxxxxxxxxxxx\n");

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "Ant Det Finish = %s, Ant Det Number  = %d\n",
		 board_info->btdm_ant_det_finish ? "Yes" : "No",
		 board_info->btdm_ant_num_by_ant_det);

	btcoexist->dbg_mode_1ant = false;
	btcoexist->stop_coex_dm = true;

	/* enable BB, REG_SYS_FUNC_EN such that we can write 0x948 correctly. */
	u16tmp = btcoexist->btc_read_2byte(btcoexist, 0x2);
	btcoexist->btc_write_2byte(btcoexist, 0x2, u16tmp | BIT(0) | BIT(1));

	/* set Path control owner to WiFi */
	halbtc8822b1ant_ltecoex_pathcontrol_owner(btcoexist,
						  BT_8822B_1ANT_PCO_WLSIDE);

	/* set GNT_BT to high */
	halbtc8822b1ant_ltecoex_set_gnt_bt(btcoexist,
					   BT_8822B_1ANT_GNT_BLOCK_RFC_BB,
					   BT_8822B_1ANT_GNT_CTRL_BY_SW,
					   BT_8822B_1ANT_SIG_STA_SET_TO_HIGH);
	/* Set GNT_WL to low */
	halbtc8822b1ant_ltecoex_set_gnt_wl(
		btcoexist, BT_8822B_1ANT_GNT_BLOCK_RFC_BB,
		BT_8822B_1ANT_GNT_CTRL_BY_SW, BT_8822B_1ANT_SIG_STA_SET_TO_LOW);

	/* set WLAN_ACT = 0 */
	/* btcoexist->btc_write_1byte(btcoexist, 0x76e, 0x4); */

	/* SD1 Chunchu red x issue */
	btcoexist->btc_write_1byte(btcoexist, 0xff1a, 0x0);

	halbtc8822b1ant_enable_gnt_to_gpio(btcoexist, true);

	/* */
	/* S0 or S1 setting and Local register setting
	 * (By the setting fw can get ant number, S0/S1, ... info)
	 */
	/* Local setting bit define */
	/*	BIT0: "0" for no antenna inverse; "1" for antenna inverse  */
	/*	BIT1: "0" for internal switch; "1" for external switch */
	/*	BIT2: "0" for one antenna; "1" for two antenna */
	/* NOTE: here default all internal switch and 1-antenna ==>
	 *       BIT1=0 and BIT2=0
	 */

	u8tmp = 0;
	board_info->btdm_ant_pos = BTC_ANTENNA_AT_MAIN_PORT;

	if (btcoexist->chip_interface == BTC_INTF_USB)
		btcoexist->btc_write_local_reg_1byte(btcoexist, 0xfe08, u8tmp);
	else if (btcoexist->chip_interface == BTC_INTF_SDIO)
		btcoexist->btc_write_local_reg_1byte(btcoexist, 0x60, u8tmp);
}

void ex_btc8822b1ant_pre_load_firmware(struct btc_coexist *btcoexist) {}

void ex_btc8822b1ant_init_hw_config(struct btc_coexist *btcoexist,
				    bool wifi_only)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex], ********** (ini hw config) **********\n");

	halbtc8822b1ant_init_hw_config(btcoexist, true, wifi_only);
	btcoexist->stop_coex_dm = false;
	btcoexist->auto_report_1ant = true;
}

void ex_btc8822b1ant_init_coex_dm(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex], Coex Mechanism Init!!\n");

	btcoexist->stop_coex_dm = false;

	halbtc8822b1ant_init_coex_dm(btcoexist);

	halbtc8822b1ant_query_bt_info(btcoexist);
}

void ex_btc8822b1ant_display_coex_info(struct btc_coexist *btcoexist,
				       struct seq_file *m)
{
	struct btc_board_info *board_info = &btcoexist->board_info;
	struct btc_bt_link_info *bt_link_info = &btcoexist->bt_link_info;

	u8 u8tmp[4], i, ps_tdma_case = 0;
	u16 u16tmp[4];
	u32 u32tmp[4];
	u32 fa_ofdm, fa_cck, cca_ofdm, cca_cck;
	u32 fw_ver = 0, bt_patch_ver = 0, bt_coex_ver = 0;
	static u8 pop_report_in_10s;
	u32 phyver = 0;
	bool lte_coex_on = false;
	static u8 cnt;

	seq_puts(m, "\r\n ============[BT Coexist info]============");

	if (btcoexist->manual_control) {
		seq_puts(m,
			 "\r\n ============[Under Manual Control]============");
		seq_puts(m, "\r\n ==========================================");
	}
	if (btcoexist->stop_coex_dm) {
		seq_puts(m, "\r\n ============[Coex is STOPPED]============");
		seq_puts(m, "\r\n ==========================================");
	}

	if (!coex_sta->bt_disabled) {
		if (coex_sta->bt_coex_supported_feature == 0)
			btcoexist->btc_get(
				btcoexist, BTC_GET_U4_SUPPORTED_FEATURE,
				&coex_sta->bt_coex_supported_feature);

		if ((coex_sta->bt_coex_supported_version == 0) ||
		    (coex_sta->bt_coex_supported_version == 0xffff))
			btcoexist->btc_get(
				btcoexist, BTC_GET_U4_SUPPORTED_VERSION,
				&coex_sta->bt_coex_supported_version);

		if (coex_sta->bt_reg_vendor_ac == 0xffff)
			coex_sta->bt_reg_vendor_ac = (u16)(
				btcoexist->btc_get_bt_reg(btcoexist, 3, 0xac) &
				0xffff);

		if (coex_sta->bt_reg_vendor_ae == 0xffff)
			coex_sta->bt_reg_vendor_ae = (u16)(
				btcoexist->btc_get_bt_reg(btcoexist, 3, 0xae) &
				0xffff);

		btcoexist->btc_get(btcoexist, BTC_GET_U4_BT_PATCH_VER,
				   &bt_patch_ver);
		btcoexist->bt_info.bt_get_fw_ver = bt_patch_ver;

		if (coex_sta->num_of_profile > 0) {
			cnt++;

			if (cnt >= 3) {
				btcoexist->btc_get_bt_afh_map_from_bt(
					btcoexist, 0, &coex_sta->bt_afh_map[0]);
				cnt = 0;
			}
		}
	}

	if (psd_scan->ant_det_try_count == 0) {
		seq_printf(
			m, "\r\n %-35s = %d/ %d/ %s / %d",
			"Ant PG Num/ Mech/ Pos/ RFE", board_info->pg_ant_num,
			board_info->btdm_ant_num,
			(board_info->btdm_ant_pos == BTC_ANTENNA_AT_MAIN_PORT ?
				 "Main" :
				 "Aux"),
			rfe_type->rfe_module_type);
	} else {
		seq_printf(
			m, "\r\n %-35s = %d/ %d/ %s/ %d  (%d/%d/%d)",
			"Ant PG Num/ Mech(Ant_Det)/ Pos/ RFE",
			board_info->pg_ant_num,
			board_info->btdm_ant_num_by_ant_det,
			(board_info->btdm_ant_pos == BTC_ANTENNA_AT_MAIN_PORT ?
				 "Main" :
				 "Aux"),
			rfe_type->rfe_module_type, psd_scan->ant_det_try_count,
			psd_scan->ant_det_fail_count, psd_scan->ant_det_result);

		if (board_info->btdm_ant_det_finish) {
			if (psd_scan->ant_det_result != 12)
				seq_printf(m, "\r\n %-35s = %s",
					   "Ant Det PSD Value",
					   psd_scan->ant_det_peak_val);
			else
				seq_printf(m, "\r\n %-35s = %d",
					   "Ant Det PSD Value",
					   psd_scan->ant_det_psd_scan_peak_val /
						   100);
		}
	}

	bt_patch_ver = btcoexist->bt_info.bt_get_fw_ver;
	btcoexist->btc_get(btcoexist, BTC_GET_U4_WIFI_FW_VER, &fw_ver);
	phyver = btcoexist->btc_get_bt_phydm_version(btcoexist);

	bt_coex_ver = ((coex_sta->bt_coex_supported_version & 0xff00) >> 8);

	seq_printf(
		m, "\r\n %-35s = %d_%02x/ 0x%02x/ 0x%02x (%s)",
		"CoexVer WL/  BT_Desired/ BT_Report",
		glcoex_ver_date_8822b_1ant, glcoex_ver_8822b_1ant,
		glcoex_ver_btdesired_8822b_1ant, bt_coex_ver,
		(bt_coex_ver == 0xff ?
			 "Unknown" :
			 (coex_sta->bt_disabled ?  "BT-disable" :
			  (bt_coex_ver >= glcoex_ver_btdesired_8822b_1ant ?
				   "Match" :
				   "Mis-Match"))));

	seq_printf(m, "\r\n %-35s = 0x%x/ 0x%x/ v%d/ %c", "W_FW/ B_FW/ Phy/ Kt",
		   fw_ver, bt_patch_ver, phyver, coex_sta->cut_version + 65);

	seq_printf(m, "\r\n %-35s = %02x %02x %02x ", "AFH Map to BT",
		   coex_dm->wifi_chnl_info[0], coex_dm->wifi_chnl_info[1],
		   coex_dm->wifi_chnl_info[2]);

	/* wifi status */
	seq_printf(m, "\r\n %-35s", "============[Wifi Status]============");
	btcoexist->btc_disp_dbg_msg(btcoexist, BTC_DBG_DISP_WIFI_STATUS, m);

	seq_printf(m, "\r\n %-35s", "============[BT Status]============");

	pop_report_in_10s++;
	seq_printf(
		m, "\r\n %-35s = [%s/ %d dBm/ %d/ %d] ",
		"BT [status/ rssi/ retryCnt/ popCnt]",
		((coex_sta->bt_disabled) ?
			 ("disabled") :
			 ((coex_sta->c2h_bt_inquiry_page) ?  ("inquiry/page") :
			  ((BT_8822B_1ANT_BT_STATUS_NON_CONNECTED_IDLE ==
			    coex_dm->bt_status) ?
				   "non-connected idle" :
				   ((coex_dm->bt_status ==
				     BT_8822B_1ANT_BT_STATUS_CONNECTED_IDLE) ?
					    "connected-idle" :
					    "busy")))),
		coex_sta->bt_rssi - 100, coex_sta->bt_retry_cnt,
		coex_sta->pop_event_cnt);

	if (pop_report_in_10s >= 5) {
		coex_sta->pop_event_cnt = 0;
		pop_report_in_10s = 0;
	}

	if (coex_sta->num_of_profile != 0)
		seq_printf(
			m, "\r\n %-35s = %s%s%s%s%s", "Profiles",
			((bt_link_info->a2dp_exist) ?
				 ((coex_sta->is_bt_a2dp_sink) ? "A2DP sink," :
								"A2DP,") :
				 ""),
			((bt_link_info->sco_exist) ? "HFP," : ""),
			((bt_link_info->hid_exist) ?
				 ((coex_sta->hid_busy_num >= 2) ?
					  "HID(4/18)," :
					  "HID(2/18),") :
				 ""),
			((bt_link_info->pan_exist) ? "PAN," : ""),
			((coex_sta->voice_over_HOGP) ? "Voice" : ""));
	else
		seq_printf(m, "\r\n %-35s = None", "Profiles");

	if (bt_link_info->a2dp_exist) {
		seq_printf(m, "\r\n %-35s = %s/ %d/ %s",
			   "A2DP Rate/Bitpool/Auto_Slot",
			   ((coex_sta->is_A2DP_3M) ? "3M" : "No_3M"),
			   coex_sta->a2dp_bit_pool,
			   ((coex_sta->is_autoslot) ? "On" : "Off"));
	}

	if (bt_link_info->hid_exist) {
		seq_printf(m, "\r\n %-35s = %d/ %d", "HID PairNum/Forbid_Slot",
			   coex_sta->hid_pair_cnt, coex_sta->forbidden_slot);
	}

	seq_printf(m, "\r\n %-35s = %s/ %d/ %s/ 0x%x",
		   "Role/RoleSwCnt/IgnWlact/Feature",
		   ((bt_link_info->slave_role) ? "Slave" : "Master"),
		   coex_sta->cnt_role_switch,
		   ((coex_dm->cur_ignore_wlan_act) ? "Yes" : "No"),
		   coex_sta->bt_coex_supported_feature);

	if ((coex_sta->bt_ble_scan_type & 0x7) != 0x0) {
		seq_printf(m, "\r\n %-35s = 0x%x/ 0x%x/ 0x%x/ 0x%x",
			   "BLEScan Type/TV/Init/Ble",
			   coex_sta->bt_ble_scan_type,
			   (coex_sta->bt_ble_scan_type & 0x1 ?
				    coex_sta->bt_ble_scan_para[0] :
				    0x0),
			   (coex_sta->bt_ble_scan_type & 0x2 ?
				    coex_sta->bt_ble_scan_para[1] :
				    0x0),
			   (coex_sta->bt_ble_scan_type & 0x4 ?
				    coex_sta->bt_ble_scan_para[2] :
				    0x0));
	}

	seq_printf(m, "\r\n %-35s = %d/ %d/ %d/ %d/ %d",
		   "ReInit/ReLink/IgnWlact/Page/NameReq", coex_sta->cnt_reinit,
		   coex_sta->cnt_setup_link, coex_sta->cnt_ign_wlan_act,
		   coex_sta->cnt_page, coex_sta->cnt_remote_name_req);

	halbtc8822b1ant_read_score_board(btcoexist, &u16tmp[0]);

	if ((coex_sta->bt_reg_vendor_ae == 0xffff) ||
	    (coex_sta->bt_reg_vendor_ac == 0xffff))
		seq_printf(m, "\r\n %-35s = x/ x/ %04x",
			   "0xae[4]/0xac[1:0]/Scoreboard", u16tmp[0]);
	else
		seq_printf(m, "\r\n %-35s = 0x%x/ 0x%x/ %04x",
			   "0xae[4]/0xac[1:0]/Scoreboard",
			   (int)((coex_sta->bt_reg_vendor_ae & BIT(4)) >> 4),
			   coex_sta->bt_reg_vendor_ac & 0x3, u16tmp[0]);

	if (coex_sta->num_of_profile > 0) {
		seq_printf(
			m,
			"\r\n %-35s = %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
			"AFH MAP", coex_sta->bt_afh_map[0],
			coex_sta->bt_afh_map[1], coex_sta->bt_afh_map[2],
			coex_sta->bt_afh_map[3], coex_sta->bt_afh_map[4],
			coex_sta->bt_afh_map[5], coex_sta->bt_afh_map[6],
			coex_sta->bt_afh_map[7], coex_sta->bt_afh_map[8],
			coex_sta->bt_afh_map[9]);
	}

	for (i = 0; i < BT_INFO_SRC_8822B_1ANT_MAX; i++) {
		if (coex_sta->bt_info_c2h_cnt[i]) {
			seq_printf(
				m,
				"\r\n %-35s = %02x %02x %02x %02x %02x %02x %02x(%d)",
				glbt_info_src_8822b_1ant[i],
				coex_sta->bt_info_c2h[i][0],
				coex_sta->bt_info_c2h[i][1],
				coex_sta->bt_info_c2h[i][2],
				coex_sta->bt_info_c2h[i][3],
				coex_sta->bt_info_c2h[i][4],
				coex_sta->bt_info_c2h[i][5],
				coex_sta->bt_info_c2h[i][6],
				coex_sta->bt_info_c2h_cnt[i]);
		}
	}

	if (btcoexist->manual_control)
		seq_printf(
			m, "\r\n %-35s",
			"============[mechanisms] (before Manual)============");
	else
		seq_printf(m, "\r\n %-35s",
			   "============[Mechanisms]============");

	ps_tdma_case = coex_dm->cur_ps_tdma;
	seq_printf(m, "\r\n %-35s = %02x %02x %02x %02x %02x (case-%d, %s)",
		   "TDMA", coex_dm->ps_tdma_para[0], coex_dm->ps_tdma_para[1],
		   coex_dm->ps_tdma_para[2], coex_dm->ps_tdma_para[3],
		   coex_dm->ps_tdma_para[4], ps_tdma_case,
		   (coex_dm->cur_ps_tdma_on ? "TDMA On" : "TDMA Off"));

	u32tmp[0] = btcoexist->btc_read_4byte(btcoexist, 0x6c0);
	u32tmp[1] = btcoexist->btc_read_4byte(btcoexist, 0x6c4);
	u32tmp[2] = btcoexist->btc_read_4byte(btcoexist, 0x6c8);
	seq_printf(m, "\r\n %-35s = %d/ 0x%x/ 0x%x/ 0x%x",
		   "Table/0x6c0/0x6c4/0x6c8", coex_sta->coex_table_type,
		   u32tmp[0], u32tmp[1], u32tmp[2]);

	u8tmp[0] = btcoexist->btc_read_1byte(btcoexist, 0x778);
	u32tmp[0] = btcoexist->btc_read_4byte(btcoexist, 0x6cc);
	seq_printf(m, "\r\n %-35s = 0x%x/ 0x%x", "0x778/0x6cc", u8tmp[0],
		   u32tmp[0]);

	seq_printf(m, "\r\n %-35s = %s/ %s/ %s/ %d",
		   "AntDiv/BtCtrlLPS/LPRA/PsFail",
		   ((board_info->ant_div_cfg) ? "On" : "Off"),
		   ((coex_sta->force_lps_ctrl) ? "On" : "Off"),
		   ((coex_dm->cur_low_penalty_ra) ? "On" : "Off"),
		   coex_sta->cnt_set_ps_state_fail);

	u32tmp[0] = halbtc8822b1ant_ltecoex_indirect_read_reg(btcoexist, 0x38);
	lte_coex_on = ((u32tmp[0] & BIT(7)) >> 7) ? true : false;

	if (lte_coex_on) {
		u32tmp[0] = halbtc8822b1ant_ltecoex_indirect_read_reg(btcoexist,
								      0xa0);
		u32tmp[1] = halbtc8822b1ant_ltecoex_indirect_read_reg(btcoexist,
								      0xa4);

		seq_printf(m, "\r\n %-35s = 0x%x/ 0x%x",
			   "LTE Coex Table W_L/B_L", u32tmp[0] & 0xffff,
			   u32tmp[1] & 0xffff);

		u32tmp[0] = halbtc8822b1ant_ltecoex_indirect_read_reg(btcoexist,
								      0xa8);
		u32tmp[1] = halbtc8822b1ant_ltecoex_indirect_read_reg(btcoexist,
								      0xac);
		u32tmp[2] = halbtc8822b1ant_ltecoex_indirect_read_reg(btcoexist,
								      0xb0);
		u32tmp[3] = halbtc8822b1ant_ltecoex_indirect_read_reg(btcoexist,
								      0xb4);

		seq_printf(m, "\r\n %-35s = 0x%x/ 0x%x/ 0x%x/ 0x%x",
			   "LTE Break Table W_L/B_L/L_W/L_B",
			   u32tmp[0] & 0xffff, u32tmp[1] & 0xffff,
			   u32tmp[2] & 0xffff, u32tmp[3] & 0xffff);
	}

	/* Hw setting		 */
	seq_printf(m, "\r\n %-35s", "============[Hw setting]============");

	u32tmp[0] = halbtc8822b1ant_ltecoex_indirect_read_reg(btcoexist, 0x38);
	u32tmp[1] = halbtc8822b1ant_ltecoex_indirect_read_reg(btcoexist, 0x54);
	u8tmp[0] = btcoexist->btc_read_1byte(btcoexist, 0x73);

	seq_printf(m, "\r\n %-35s = %s/ %s", "LTE Coex/Path Owner",
		   ((lte_coex_on) ? "On" : "Off"),
		   ((u8tmp[0] & BIT(2)) ? "WL" : "BT"));

	if (lte_coex_on) {
		seq_printf(m, "\r\n %-35s = %d/ %d/ %d/ %d",
			   "LTE 3Wire/OPMode/UART/UARTMode",
			   (int)((u32tmp[0] & BIT(6)) >> 6),
			   (int)((u32tmp[0] & (BIT(5) | BIT(4))) >> 4),
			   (int)((u32tmp[0] & BIT(3)) >> 3),
			   (int)(u32tmp[0] & (BIT(2) | BIT(1) | BIT(0))));

		seq_printf(m, "\r\n %-35s = %d/ %d", "LTE_Busy/UART_Busy",
			   (int)((u32tmp[1] & BIT(1)) >> 1),
			   (int)(u32tmp[1] & BIT(0)));
	}
	seq_printf(m, "\r\n %-35s = %s (BB:%s)/ %s (BB:%s)/ %s %d",
		   "GNT_WL_Ctrl/GNT_BT_Ctrl/Dbg",
		   ((u32tmp[0] & BIT(12)) ? "SW" : "HW"),
		   ((u32tmp[0] & BIT(8)) ? "SW" : "HW"),
		   ((u32tmp[0] & BIT(14)) ? "SW" : "HW"),
		   ((u32tmp[0] & BIT(10)) ? "SW" : "HW"),
		   ((u8tmp[0] & BIT(3)) ? "On" : "Off"),
		   coex_sta->gnt_error_cnt);

	seq_printf(m, "\r\n %-35s = %d/ %d", "GNT_WL/GNT_BT",
		   (int)((u32tmp[1] & BIT(2)) >> 2),
		   (int)((u32tmp[1] & BIT(3)) >> 3));

	u32tmp[0] = btcoexist->btc_read_4byte(btcoexist, 0xcb0);
	u32tmp[1] = btcoexist->btc_read_4byte(btcoexist, 0xcb4);
	u8tmp[0] = btcoexist->btc_read_1byte(btcoexist, 0xcba);

	seq_printf(m, "\r\n %-35s = 0x%04x/ 0x%04x/ 0x%02x %s",
		   "0xcb0/0xcb4/0xcb8[23:16]", u32tmp[0], u32tmp[1], u8tmp[0],
		   ((u8tmp[0] & 0x1) == 0x1 ? "(BTG)" : "(WL_A+G)"));

	u32tmp[0] = btcoexist->btc_read_4byte(btcoexist, 0x4c);
	u8tmp[2] = btcoexist->btc_read_1byte(btcoexist, 0x64);
	u8tmp[0] = btcoexist->btc_read_1byte(btcoexist, 0x4c6);
	u8tmp[1] = btcoexist->btc_read_1byte(btcoexist, 0x40);

	seq_printf(m, "\r\n %-35s = 0x%x/ 0x%x/ 0x%x/ 0x%x",
		   "4c[24:23]/64[0]/4c6[4]/40[5]",
		   (int)((u32tmp[0] & (BIT(24) | BIT(23))) >> 23),
		   u8tmp[2] & 0x1, (int)((u8tmp[0] & BIT(4)) >> 4),
		   (int)((u8tmp[1] & BIT(5)) >> 5));

	u32tmp[0] = btcoexist->btc_read_4byte(btcoexist, 0x550);
	u8tmp[0] = btcoexist->btc_read_1byte(btcoexist, 0x522);
	u8tmp[1] = btcoexist->btc_read_1byte(btcoexist, 0x953);
	u8tmp[2] = btcoexist->btc_read_1byte(btcoexist, 0xc50);

	seq_printf(m, "\r\n %-35s = 0x%x/ 0x%x/ %s/ 0x%x",
		   "0x550/0x522/4-RxAGC/0xc50", u32tmp[0], u8tmp[0],
		   (u8tmp[1] & 0x2) ? "On" : "Off", u8tmp[2]);

	fa_ofdm = btcoexist->btc_phydm_query_phy_counter(btcoexist,
							 "PHYDM_INFO_FA_OFDM");
	fa_cck = btcoexist->btc_phydm_query_phy_counter(btcoexist,
							"PHYDM_INFO_FA_CCK");
	cca_ofdm = btcoexist->btc_phydm_query_phy_counter(
		btcoexist, "PHYDM_INFO_CCA_OFDM");
	cca_cck = btcoexist->btc_phydm_query_phy_counter(btcoexist,
							 "PHYDM_INFO_CCA_CCK");

	seq_printf(m, "\r\n %-35s = 0x%x/ 0x%x/ 0x%x/ 0x%x",
		   "CCK-CCA/CCK-FA/OFDM-CCA/OFDM-FA", cca_cck, fa_cck, cca_ofdm,
		   fa_ofdm);

	seq_printf(m, "\r\n %-35s = %d/ %d/ %d/ %d", "CRC_OK CCK/11g/11n/11ac",
		   coex_sta->crc_ok_cck, coex_sta->crc_ok_11g,
		   coex_sta->crc_ok_11n, coex_sta->crc_ok_11n_vht);

	seq_printf(m, "\r\n %-35s = %d/ %d/ %d/ %d", "CRC_Err CCK/11g/11n/11ac",
		   coex_sta->crc_err_cck, coex_sta->crc_err_11g,
		   coex_sta->crc_err_11n, coex_sta->crc_err_11n_vht);

	seq_printf(m, "\r\n %-35s = %s/ %s/ %s/ %d",
		   "WlHiPri/ Locking/ Locked/ Noisy",
		   (coex_sta->wifi_is_high_pri_task ? "Yes" : "No"),
		   (coex_sta->cck_lock ? "Yes" : "No"),
		   (coex_sta->cck_ever_lock ? "Yes" : "No"),
		   coex_sta->wl_noisy_level);

	seq_printf(m, "\r\n %-35s = %d/ %d", "0x770(Hi-pri rx/tx)",
		   coex_sta->high_priority_rx, coex_sta->high_priority_tx);

	seq_printf(m, "\r\n %-35s = %d/ %d %s", "0x774(Lo-pri rx/tx)",
		   coex_sta->low_priority_rx, coex_sta->low_priority_tx,
		   (bt_link_info->slave_role ?
			    "(Slave!!)" :
			    (coex_sta->is_tdma_btautoslot_hang ?
				     "(auto-slot hang!!)" :
				     "")));

	btcoexist->btc_disp_dbg_msg(btcoexist, BTC_DBG_DISP_COEX_STATISTICS, m);
}

void ex_btc8822b1ant_ips_notify(struct btc_coexist *btcoexist, u8 type)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;

	if (btcoexist->manual_control || btcoexist->stop_coex_dm)
		return;

	if (type == BTC_IPS_ENTER) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], IPS ENTER notify\n");
		coex_sta->under_ips = true;

		/* Write WL "Active" in Score-board for LPS off */
		halbtc8822b1ant_post_state_to_bt(
			btcoexist, BT_8822B_1ANT_SCOREBOARD_ACTIVE, false);

		halbtc8822b1ant_post_state_to_bt(
			btcoexist, BT_8822B_1ANT_SCOREBOARD_ONOFF, false);

		halbtc8822b1ant_ps_tdma(btcoexist, NORMAL_EXEC, false, 0);

		halbtc8822b1ant_set_ant_path(btcoexist, BTC_ANT_PATH_AUTO,
					     FORCE_EXEC,
					     BT_8822B_1ANT_PHASE_WLAN_OFF);

		halbtc8822b1ant_coex_table_with_type(btcoexist, NORMAL_EXEC, 0);
	} else if (type == BTC_IPS_LEAVE) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], IPS LEAVE notify\n");
		halbtc8822b1ant_post_state_to_bt(
			btcoexist, BT_8822B_1ANT_SCOREBOARD_ACTIVE, true);

		halbtc8822b1ant_post_state_to_bt(
			btcoexist, BT_8822B_1ANT_SCOREBOARD_ONOFF, true);

		/*leave IPS : run ini hw config (exclude wifi only)*/
		halbtc8822b1ant_init_hw_config(btcoexist, false, false);
		/*sw all off*/
		halbtc8822b1ant_init_coex_dm(btcoexist);
		/*leave IPS : Query bt info*/
		halbtc8822b1ant_query_bt_info(btcoexist);

		coex_sta->under_ips = false;
	}
}

void ex_btc8822b1ant_lps_notify(struct btc_coexist *btcoexist, u8 type)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	static bool pre_force_lps_on;

	if (btcoexist->manual_control || btcoexist->stop_coex_dm)
		return;

	if (type == BTC_LPS_ENABLE) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], LPS ENABLE notify\n");
		coex_sta->under_lps = true;

		if (coex_sta->force_lps_ctrl) { /* LPS No-32K */
			/* Write WL "Active" in Score-board for PS-TDMA */
			pre_force_lps_on = true;
			halbtc8822b1ant_post_state_to_bt(
				btcoexist, BT_8822B_1ANT_SCOREBOARD_ACTIVE,
				true);
		} else {
			/* LPS-32K, need check if this h2c 0x71 can work??
			 * (2015/08/28)
			 */
			/* Write WL "Non-Active" in Score-board for Native-PS */
			pre_force_lps_on = false;
			halbtc8822b1ant_post_state_to_bt(
				btcoexist, BT_8822B_1ANT_SCOREBOARD_ACTIVE,
				false);
		}
	} else if (type == BTC_LPS_DISABLE) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], LPS DISABLE notify\n");
		coex_sta->under_lps = false;

		/* Write WL "Active" in Score-board for LPS off */
		halbtc8822b1ant_post_state_to_bt(
			btcoexist, BT_8822B_1ANT_SCOREBOARD_ACTIVE, true);

		if ((!pre_force_lps_on) && (!coex_sta->force_lps_ctrl))
			halbtc8822b1ant_query_bt_info(btcoexist);
	}
}

void ex_btc8822b1ant_scan_notify(struct btc_coexist *btcoexist, u8 type)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	bool wifi_connected = false;
	bool wifi_under_5g = false;

	if (btcoexist->manual_control || btcoexist->stop_coex_dm)
		return;

	coex_sta->freeze_coexrun_by_btinfo = false;

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_CONNECTED,
			   &wifi_connected);

	if (wifi_connected)
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], ********** WL connected before SCAN\n");
	else
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], **********  WL is not connected before SCAN\n");

	halbtc8822b1ant_query_bt_info(btcoexist);

	/*2.4 g 1*/
	if (type == BTC_SCAN_START) {
		btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_UNDER_5G,
				   &wifi_under_5g);
		/*5 g 1*/

		if (wifi_under_5g) {
			RT_TRACE(
				rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				"[BTCoex], ********** (scan_notify_5g_scan_start) **********\n");
			halbtc8822b1ant_action_wifi_under5g(btcoexist);
			return;
		}

		/* 2.4G.2.3*/
		coex_sta->wifi_is_high_pri_task = true;

		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], ********** (scan_notify_2g_scan_start) **********\n");

		if (!wifi_connected) { /* non-connected scan */
			RT_TRACE(
				rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				"[BTCoex], ********** wifi is not connected scan **********\n");
			halbtc8822b1ant_action_wifi_not_connected_scan(
				btcoexist);
		} else { /* wifi is connected */
			RT_TRACE(
				rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				"[BTCoex], ********** wifi is connected scan **********\n");
			halbtc8822b1ant_action_wifi_connected_scan(btcoexist);
		}

		return;
	}

	if (type == BTC_SCAN_START_2G) {
		coex_sta->wifi_is_high_pri_task = true;

		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], ********** (scan_notify_2g_sacn_start_for_switch_band_used) **********\n");

		if (!wifi_connected) { /* non-connected scan */
			RT_TRACE(
				rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				"[BTCoex], ********** wifi is not connected **********\n");

			halbtc8822b1ant_action_wifi_not_connected_scan(
				btcoexist);
		} else { /* wifi is connected */
			RT_TRACE(
				rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				"[BTCoex], ********** wifi is connected **********\n");
			halbtc8822b1ant_action_wifi_connected_scan(btcoexist);
		}
	} else {
		coex_sta->wifi_is_high_pri_task = false;

		/* 2.4G 5 WL scan finish, then get and update sacn ap numbers */
		/*5 g 4*/
		btcoexist->btc_get(btcoexist, BTC_GET_U1_AP_NUM,
				   &coex_sta->scan_ap_num);

		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], ********** (scan_finish_notify) **********\n");

		if (!wifi_connected) { /* non-connected scan */
			halbtc8822b1ant_action_wifi_not_connected(btcoexist);
		} else {
			RT_TRACE(
				rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				"[BTCoex], ********** scan_finish_notify wifi is connected **********\n");
			halbtc8822b1ant_action_wifi_connected(btcoexist);
		}
	}
}

void ex_btc8822b1ant_scan_notify_without_bt(struct btc_coexist *btcoexist,
					    u8 type)
{
	bool wifi_under_5g = false;

	if (type == BTC_SCAN_START) {
		btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_UNDER_5G,
				   &wifi_under_5g);

		if (wifi_under_5g) {
			btcoexist->btc_write_1byte_bitmask(btcoexist, 0xcbd,
							   0x3, 1);
			return;
		}

		/* under 2.4G */
		btcoexist->btc_write_1byte_bitmask(btcoexist, 0xcbd, 0x3, 2);
		return;
	}
	if (type == BTC_SCAN_START_2G)
		btcoexist->btc_write_1byte_bitmask(btcoexist, 0xcbd, 0x3, 2);
}

void ex_btc8822b1ant_switchband_notify(struct btc_coexist *btcoexist, u8 type)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex], ********** (switchband_notify) **********\n");

	if (btcoexist->manual_control || btcoexist->stop_coex_dm)
		return;

	coex_sta->switch_band_notify_to = type;
	/*2.4g 4.*/ /*5 g 2*/
	if (type == BTC_SWITCH_TO_5G) {
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], ********** (switchband_notify BTC_SWITCH_TO_5G) **********\n");

		halbtc8822b1ant_action_wifi_under5g(btcoexist);
		return;
	} else if (type == BTC_SWITCH_TO_24G_NOFORSCAN) {
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], ********** (switchband_notify BTC_SWITCH_TO_2G (no for scan)) **********\n");

		halbtc8822b1ant_run_coexist_mechanism(btcoexist);
		/*5 g 3*/

	} else {
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], ********** (switchband_notify BTC_SWITCH_TO_2G) **********\n");

		ex_btc8822b1ant_scan_notify(btcoexist, BTC_SCAN_START_2G);
	}
	coex_sta->switch_band_notify_to = BTC_NOT_SWITCH;
}

void ex_btc8822b1ant_switchband_notify_without_bt(struct btc_coexist *btcoexist,
						  u8 type)
{
	bool wifi_under_5g = false;

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_UNDER_5G, &wifi_under_5g);

	if (type == BTC_SWITCH_TO_5G) {
		btcoexist->btc_write_1byte_bitmask(btcoexist, 0xcbd, 0x3, 1);
		return;
	} else if (type == BTC_SWITCH_TO_24G_NOFORSCAN) {
		if (wifi_under_5g)

			btcoexist->btc_write_1byte_bitmask(btcoexist, 0xcbd,
							   0x3, 1);

		else
			btcoexist->btc_write_1byte_bitmask(btcoexist, 0xcbd,
							   0x3, 2);
	} else {
		ex_btc8822b1ant_scan_notify_without_bt(btcoexist,
						       BTC_SCAN_START_2G);
	}
}

void ex_btc8822b1ant_connect_notify(struct btc_coexist *btcoexist, u8 type)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	bool wifi_connected = false;

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex], ********** (connect notify) **********\n");

	halbtc8822b1ant_post_state_to_bt(btcoexist,
					 BT_8822B_1ANT_SCOREBOARD_SCAN, true);

	if (btcoexist->manual_control || btcoexist->stop_coex_dm)
		return;

	if ((type == BTC_ASSOCIATE_5G_START) ||
	    (type == BTC_ASSOCIATE_5G_FINISH)) {
		if (type == BTC_ASSOCIATE_5G_START) {
			RT_TRACE(
				rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				"[BTCoex], ********** (5G associate start notify) **********\n");

			halbtc8822b1ant_action_wifi_under5g(btcoexist);

		} else if (type == BTC_ASSOCIATE_5G_FINISH) {
			RT_TRACE(
				rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				"[BTCoex], ********** (5G associate finish notify) **********\n");
		}

		return;
	}

	if (type == BTC_ASSOCIATE_START) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], 2G CONNECT START notify\n");

		coex_sta->wifi_is_high_pri_task = true;

		halbtc8822b1ant_set_ant_path(btcoexist, BTC_ANT_PATH_AUTO,
					     FORCE_EXEC,
					     BT_8822B_1ANT_PHASE_2G_RUNTIME);

		coex_dm->arp_cnt = 0;

		halbtc8822b1ant_action_wifi_not_connected_asso_auth(btcoexist);

		coex_sta->freeze_coexrun_by_btinfo = true;

	} else {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], 2G CONNECT Finish notify\n");
		coex_sta->wifi_is_high_pri_task = false;
		coex_sta->freeze_coexrun_by_btinfo = false;

		btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_CONNECTED,
				   &wifi_connected);

		if (!wifi_connected) /* non-connected scan */
			halbtc8822b1ant_action_wifi_not_connected(btcoexist);
		else
			halbtc8822b1ant_action_wifi_connected(btcoexist);
	}
}

void ex_btc8822b1ant_media_status_notify(struct btc_coexist *btcoexist, u8 type)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	bool wifi_under_b_mode = false;
	bool wifi_under_5g = false;

	if (btcoexist->manual_control || btcoexist->stop_coex_dm)
		return;

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_UNDER_5G, &wifi_under_5g);

	if (type == BTC_MEDIA_CONNECT) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], 2g media connect notify");

		halbtc8822b1ant_post_state_to_bt(
			btcoexist, BT_8822B_1ANT_SCOREBOARD_ACTIVE, true);

		if (wifi_under_5g) {
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "[BTCoex], 5g media notify\n");

			halbtc8822b1ant_action_wifi_under5g(btcoexist);
			return;
		}
		/* Force antenna setup for no scan result issue */
		halbtc8822b1ant_set_ant_path(btcoexist, BTC_ANT_PATH_AUTO,
					     FORCE_EXEC,
					     BT_8822B_1ANT_PHASE_2G_RUNTIME);

		btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_UNDER_B_MODE,
				   &wifi_under_b_mode);

		/* Set CCK Tx/Rx high Pri except 11b mode */
		if (wifi_under_b_mode) {
			RT_TRACE(
				rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				"[BTCoex], ********** (media status notity under b mode) **********\n");
			btcoexist->btc_write_1byte(btcoexist, 0x6cd,
						   0x00); /* CCK Tx */
			btcoexist->btc_write_1byte(btcoexist, 0x6cf,
						   0x00); /* CCK Rx */
		} else {
			RT_TRACE(
				rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				"[BTCoex], ********** (media status notity not under b mode) **********\n");
			btcoexist->btc_write_1byte(btcoexist, 0x6cd,
						   0x00); /* CCK Tx */
			btcoexist->btc_write_1byte(btcoexist, 0x6cf,
						   0x10); /* CCK Rx */
		}

		coex_dm->backup_arfr_cnt1 =
			btcoexist->btc_read_4byte(btcoexist, 0x430);
		coex_dm->backup_arfr_cnt2 =
			btcoexist->btc_read_4byte(btcoexist, 0x434);
		coex_dm->backup_retry_limit =
			btcoexist->btc_read_2byte(btcoexist, 0x42a);
		coex_dm->backup_ampdu_max_time =
			btcoexist->btc_read_1byte(btcoexist, 0x456);
	} else {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], 2g media disconnect notify\n");
		coex_dm->arp_cnt = 0;

		halbtc8822b1ant_post_state_to_bt(
			btcoexist, BT_8822B_1ANT_SCOREBOARD_ACTIVE, false);

		btcoexist->btc_write_1byte(btcoexist, 0x6cd, 0x0); /* CCK Tx */
		btcoexist->btc_write_1byte(btcoexist, 0x6cf, 0x0); /* CCK Rx */

		coex_sta->cck_ever_lock = false;
	}

	halbtc8822b1ant_update_wifi_ch_info(btcoexist, type);
}

void ex_btc8822b1ant_specific_packet_notify(struct btc_coexist *btcoexist,
					    u8 type)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	bool under_4way = false, wifi_under_5g = false;

	if (btcoexist->manual_control || btcoexist->stop_coex_dm)
		return;

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_UNDER_5G, &wifi_under_5g);
	if (wifi_under_5g) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], 5g special packet notify\n");

		halbtc8822b1ant_action_wifi_under5g(btcoexist);
		return;
	}

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_4_WAY_PROGRESS,
			   &under_4way);

	if (under_4way) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], specific Packet ---- under_4way!!\n");

		coex_sta->wifi_is_high_pri_task = true;
		coex_sta->specific_pkt_period_cnt = 2;
	} else if (type == BTC_PACKET_ARP) {
		coex_dm->arp_cnt++;

		if (coex_sta->wifi_is_high_pri_task) {
			RT_TRACE(
				rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				"[BTCoex], specific Packet ARP notify -cnt = %d\n",
				coex_dm->arp_cnt);
		}

	} else {
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], specific Packet DHCP or EAPOL notify [Type = %d]\n",
			type);

		coex_sta->wifi_is_high_pri_task = true;
		coex_sta->specific_pkt_period_cnt = 2;
	}

	if (coex_sta->wifi_is_high_pri_task)
		halbtc8822b1ant_action_wifi_connected_specific_packet(
			btcoexist);
}

void ex_btc8822b1ant_bt_info_notify(struct btc_coexist *btcoexist, u8 *tmp_buf,
				    u8 length)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	u8 i, rsp_source = 0;
	bool wifi_connected = false;
	bool wifi_scan = false, wifi_link = false, wifi_roam = false,
	     wifi_busy = false;
	static bool is_scoreboard_scan;

	if (psd_scan->is_ant_det_running) {
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], bt_info_notify return for AntDet is running\n");
		return;
	}

	rsp_source = tmp_buf[0] & 0xf;
	if (rsp_source >= BT_INFO_SRC_8822B_1ANT_MAX)
		rsp_source = BT_INFO_SRC_8822B_1ANT_WIFI_FW;
	coex_sta->bt_info_c2h_cnt[rsp_source]++;

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex], Bt_info[%d], len=%d, data=[", rsp_source, length);

	for (i = 0; i < length; i++) {
		coex_sta->bt_info_c2h[rsp_source][i] = tmp_buf[i];

		if (i == length - 1) {
			/* last one */
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "0x%02x]\n", tmp_buf[i]);
		} else {
			/* normal */
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD, "0x%02x, ",
				 tmp_buf[i]);
		}
	}

	coex_sta->bt_info = coex_sta->bt_info_c2h[rsp_source][1];
	coex_sta->bt_info_ext = coex_sta->bt_info_c2h[rsp_source][4];
	coex_sta->bt_info_ext2 = coex_sta->bt_info_c2h[rsp_source][5];

	if (rsp_source != BT_INFO_SRC_8822B_1ANT_WIFI_FW) {
		/* if 0xff, it means BT is under WHCK test */
		coex_sta->bt_whck_test =
			((coex_sta->bt_info == 0xff) ? true : false);

		coex_sta->bt_create_connection =
			((coex_sta->bt_info_c2h[rsp_source][2] & 0x80) ? true :
									 false);

		/* unit: %, value-100 to translate to unit: dBm */
		coex_sta->bt_rssi =
			coex_sta->bt_info_c2h[rsp_source][3] * 2 + 10;

		coex_sta->c2h_bt_remote_name_req =
			((coex_sta->bt_info_c2h[rsp_source][2] & 0x20) ? true :
									 false);

		coex_sta->is_A2DP_3M =
			((coex_sta->bt_info_c2h[rsp_source][2] & 0x10) ? true :
									 false);

		coex_sta->acl_busy =
			((coex_sta->bt_info_c2h[rsp_source][1] & 0x9) ? true :
									false);

		coex_sta->voice_over_HOGP =
			((coex_sta->bt_info_ext & 0x10) ? true : false);

		coex_sta->c2h_bt_inquiry_page =
			((coex_sta->bt_info & BT_INFO_8822B_1ANT_B_INQ_PAGE) ?
				 true :
				 false);

		coex_sta->a2dp_bit_pool =
			(((coex_sta->bt_info_c2h[rsp_source][1] & 0x49) ==
			  0x49) ?
				 (coex_sta->bt_info_c2h[rsp_source][6] & 0x7f) :
				 0);

		coex_sta->is_bt_a2dp_sink =
			(coex_sta->bt_info_c2h[rsp_source][6] & 0x80) ? true :
									false;

		coex_sta->bt_retry_cnt =
			coex_sta->bt_info_c2h[rsp_source][2] & 0xf;

		coex_sta->is_autoslot = coex_sta->bt_info_ext2 & 0x8;

		coex_sta->forbidden_slot = coex_sta->bt_info_ext2 & 0x7;

		coex_sta->hid_busy_num = (coex_sta->bt_info_ext2 & 0x30) >> 4;

		coex_sta->hid_pair_cnt = (coex_sta->bt_info_ext2 & 0xc0) >> 6;
		if (coex_sta->bt_retry_cnt >= 1)
			coex_sta->pop_event_cnt++;

		if (coex_sta->c2h_bt_remote_name_req)
			coex_sta->cnt_remote_name_req++;

		if (coex_sta->bt_info_ext & BIT(1))
			coex_sta->cnt_reinit++;

		if (coex_sta->bt_info_ext & BIT(2)) {
			coex_sta->cnt_setup_link++;
			coex_sta->is_setup_link = true;
			coex_sta->bt_relink_downcount = 2;
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "[BTCoex], Re-Link start in BT info!!\n");
		} else {
			coex_sta->is_setup_link = false;
			coex_sta->bt_relink_downcount = 0;
			RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
				 "[BTCoex], Re-Link stop in BT info!!\n");
		}

		if (coex_sta->bt_info_ext & BIT(3))
			coex_sta->cnt_ign_wlan_act++;

		if (coex_sta->bt_info_ext & BIT(6))
			coex_sta->cnt_role_switch++;

		if (coex_sta->bt_info_ext & BIT(7))
			coex_sta->is_bt_multi_link = true;
		else
			coex_sta->is_bt_multi_link = false;

		if (coex_sta->bt_create_connection) {
			coex_sta->cnt_page++;

			btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_BUSY,
					   &wifi_busy);

			btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_SCAN,
					   &wifi_scan);
			btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_LINK,
					   &wifi_link);
			btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_ROAM,
					   &wifi_roam);

			if ((wifi_link) || (wifi_roam) || (wifi_scan) ||
			    (coex_sta->wifi_is_high_pri_task) || (wifi_busy)) {
				is_scoreboard_scan = true;
				halbtc8822b1ant_post_state_to_bt(
					btcoexist,
					BT_8822B_1ANT_SCOREBOARD_SCAN, true);

			} else {
				halbtc8822b1ant_post_state_to_bt(
					btcoexist,
					BT_8822B_1ANT_SCOREBOARD_SCAN, false);
			}
		} else {
			if (is_scoreboard_scan) {
				halbtc8822b1ant_post_state_to_bt(
					btcoexist,
					BT_8822B_1ANT_SCOREBOARD_SCAN, false);
				is_scoreboard_scan = false;
			}
		}

		/* Here we need to resend some wifi info to BT */
		/* because bt is reset and loss of the info. */

		if ((!btcoexist->manual_control) &&
		    (!btcoexist->stop_coex_dm)) {
			btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_CONNECTED,
					   &wifi_connected);

			/*  Re-Init */
			if ((coex_sta->bt_info_ext & BIT(1))) {
				RT_TRACE(
					rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
					"[BTCoex], BT ext info bit1 check, send wifi BW&Chnl to BT!!\n");
				if (wifi_connected)
					halbtc8822b1ant_update_wifi_ch_info(
						btcoexist, BTC_MEDIA_CONNECT);
				else
					halbtc8822b1ant_update_wifi_ch_info(
						btcoexist,
						BTC_MEDIA_DISCONNECT);
			}

			/*	If Ignore_WLanAct && not SetUp_Link */
			if ((coex_sta->bt_info_ext & BIT(3)) &&
			    (!(coex_sta->bt_info_ext & BIT(2)))) {
				RT_TRACE(
					rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
					"[BTCoex], BT ext info bit3 check, set BT NOT to ignore Wlan active!!\n");
				halbtc8822b1ant_ignore_wlan_act(
					btcoexist, FORCE_EXEC, false);
			}
		}
	}

	if ((coex_sta->bt_info_ext & BIT(5))) {
		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], BT ext info bit4 check, query BLE Scan type!!\n");
		coex_sta->bt_ble_scan_type =
			btcoexist->btc_get_ble_scan_type_from_bt(btcoexist);

		if ((coex_sta->bt_ble_scan_type & 0x1) == 0x1)
			coex_sta->bt_ble_scan_para[0] =
				btcoexist->btc_get_ble_scan_para_from_bt(
					btcoexist, 0x1);
		if ((coex_sta->bt_ble_scan_type & 0x2) == 0x2)
			coex_sta->bt_ble_scan_para[1] =
				btcoexist->btc_get_ble_scan_para_from_bt(
					btcoexist, 0x2);
		if ((coex_sta->bt_ble_scan_type & 0x4) == 0x4)
			coex_sta->bt_ble_scan_para[2] =
				btcoexist->btc_get_ble_scan_para_from_bt(
					btcoexist, 0x4);
	}

	halbtc8822b1ant_update_bt_link_info(btcoexist);

	halbtc8822b1ant_run_coexist_mechanism(btcoexist);
}

void ex_btc8822b1ant_rf_status_notify(struct btc_coexist *btcoexist, u8 type)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex], RF Status notify\n");

	if (type == BTC_RF_ON) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], RF is turned ON!!\n");
		btcoexist->stop_coex_dm = false;

		halbtc8822b1ant_post_state_to_bt(
			btcoexist, BT_8822B_1ANT_SCOREBOARD_ACTIVE, true);
		halbtc8822b1ant_post_state_to_bt(
			btcoexist, BT_8822B_1ANT_SCOREBOARD_ONOFF, true);

	} else if (type == BTC_RF_OFF) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], RF is turned OFF!!\n");

		halbtc8822b1ant_post_state_to_bt(
			btcoexist, BT_8822B_1ANT_SCOREBOARD_ACTIVE, false);
		halbtc8822b1ant_post_state_to_bt(
			btcoexist, BT_8822B_1ANT_SCOREBOARD_ONOFF, false);
		halbtc8822b1ant_ps_tdma(btcoexist, FORCE_EXEC, false, 0);

		halbtc8822b1ant_set_ant_path(btcoexist, BTC_ANT_PATH_AUTO,
					     FORCE_EXEC,
					     BT_8822B_1ANT_PHASE_WLAN_OFF);
		/* for test : s3 bt disppear , fail rate 1/600*/

		halbtc8822b1ant_ignore_wlan_act(btcoexist, FORCE_EXEC, true);

		btcoexist->stop_coex_dm = true;
	}
}

void ex_btc8822b1ant_halt_notify(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD, "[BTCoex], Halt notify\n");

	halbtc8822b1ant_post_state_to_bt(
		btcoexist, BT_8822B_1ANT_SCOREBOARD_ACTIVE, false);
	halbtc8822b1ant_post_state_to_bt(btcoexist,
					 BT_8822B_1ANT_SCOREBOARD_ONOFF, false);

	halbtc8822b1ant_ps_tdma(btcoexist, FORCE_EXEC, false, 0);

	halbtc8822b1ant_set_ant_path(btcoexist, BTC_ANT_PATH_AUTO, FORCE_EXEC,
				     BT_8822B_1ANT_PHASE_WLAN_OFF);
	/* for test : s3 bt disppear , fail rate 1/600*/

	halbtc8822b1ant_ignore_wlan_act(btcoexist, FORCE_EXEC, true);

	ex_btc8822b1ant_media_status_notify(btcoexist, BTC_MEDIA_DISCONNECT);
	btcoexist->stop_coex_dm = true;
}

void ex_btc8822b1ant_pnp_notify(struct btc_coexist *btcoexist, u8 pnp_state)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	bool wifi_under_5g = false;

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD, "[BTCoex], Pnp notify\n");

	btcoexist->btc_get(btcoexist, BTC_GET_BL_WIFI_UNDER_5G, &wifi_under_5g);

	if ((pnp_state == BTC_WIFI_PNP_SLEEP) ||
	    (pnp_state == BTC_WIFI_PNP_SLEEP_KEEP_ANT)) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], Pnp notify to SLEEP\n");

		halbtc8822b1ant_post_state_to_bt(
			btcoexist, BT_8822B_1ANT_SCOREBOARD_ACTIVE |
					   BT_8822B_1ANT_SCOREBOARD_ONOFF |
					   BT_8822B_1ANT_SCOREBOARD_SCAN |
					   BT_8822B_1ANT_SCOREBOARD_UNDERTEST,
			false);

		if (pnp_state == BTC_WIFI_PNP_SLEEP_KEEP_ANT) {
			if (wifi_under_5g)
				halbtc8822b1ant_set_ant_path(
					btcoexist, BTC_ANT_PATH_AUTO,
					FORCE_EXEC,
					BT_8822B_1ANT_PHASE_5G_RUNTIME);
			else
				halbtc8822b1ant_set_ant_path(
					btcoexist, BTC_ANT_PATH_AUTO,
					FORCE_EXEC,
					BT_8822B_1ANT_PHASE_2G_RUNTIME);
		} else {
			halbtc8822b1ant_set_ant_path(
				btcoexist, BTC_ANT_PATH_AUTO, FORCE_EXEC,
				BT_8822B_1ANT_PHASE_WLAN_OFF);
		}

		btcoexist->stop_coex_dm = true;
	} else if (pnp_state == BTC_WIFI_PNP_WAKE_UP) {
		RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			 "[BTCoex], Pnp notify to WAKE UP\n");
		btcoexist->stop_coex_dm = false;
	}
}

void ex_btc8822b1ant_coex_dm_reset(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;

	RT_TRACE(rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		 "[BTCoex], *****************Coex DM Reset*****************\n");

	halbtc8822b1ant_init_hw_config(btcoexist, false, false);
	halbtc8822b1ant_init_coex_dm(btcoexist);
}

void ex_btc8822b1ant_periodical(struct btc_coexist *btcoexist)
{
	struct rtl_priv *rtlpriv = btcoexist->adapter;
	bool bt_relink_finish = false;

	RT_TRACE(
		rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
		"[BTCoex], ==========================Periodical===========================\n");

	if (!btcoexist->auto_report_1ant)
		halbtc8822b1ant_query_bt_info(btcoexist);

	halbtc8822b1ant_monitor_bt_ctr(btcoexist);
	halbtc8822b1ant_monitor_wifi_ctr(btcoexist);

	halbtc8822b1ant_monitor_bt_enable_disable(btcoexist);

	if (coex_sta->bt_relink_downcount != 0) {
		coex_sta->bt_relink_downcount--;

		if (coex_sta->bt_relink_downcount == 0) {
			coex_sta->is_setup_link = false;
			bt_relink_finish = true;
		}
	}

	/* for 4-way, DHCP, EAPOL packet */
	if (coex_sta->specific_pkt_period_cnt > 0) {
		coex_sta->specific_pkt_period_cnt--;

		if ((coex_sta->specific_pkt_period_cnt == 0) &&
		    (coex_sta->wifi_is_high_pri_task))
			coex_sta->wifi_is_high_pri_task = false;

		RT_TRACE(
			rtlpriv, COMP_BT_COEXIST, DBG_LOUD,
			"[BTCoex], ***************** Hi-Pri Task = %s*****************\n",
			(coex_sta->wifi_is_high_pri_task ? "Yes" : "No"));
	}

	if (halbtc8822b1ant_is_wifi_status_changed(btcoexist) ||
	    (bt_relink_finish) || (coex_sta->is_set_ps_state_fail))
		halbtc8822b1ant_run_coexist_mechanism(btcoexist);
}

void ex_btc8822b1ant_antenna_detection(struct btc_coexist *btcoexist,
				       u32 cent_freq, u32 offset, u32 span,
				       u32 seconds)
{
}

void ex_btc8822b1ant_antenna_isolation(struct btc_coexist *btcoexist,
				       u32 cent_freq, u32 offset, u32 span,
				       u32 seconds)
{
}

void ex_btc8822b1ant_psd_scan(struct btc_coexist *btcoexist, u32 cent_freq,
			      u32 offset, u32 span, u32 seconds)
{
}

void ex_btc8822b1ant_display_ant_detection(struct btc_coexist *btcoexist) {}

void ex_btc8822b1ant_dbg_control(struct btc_coexist *btcoexist, u8 op_code,
				 u8 op_len, u8 *pdata)
{
}
