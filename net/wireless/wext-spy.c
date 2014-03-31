/*
 * This file implement the Wireless Extensions spy API.
 *
 * Authors :	Jean Tourrilhes - HPL - <jt@hpl.hp.com>
 * Copyright (c) 1997-2007 Jean Tourrilhes, All Rights Reserved.
 *
 * (As all part of the Linux kernel, this file is GPL)
 */

#include <linux/wireless.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/export.h>
#include <net/iw_handler.h>
#include <net/arp.h>
#include <net/wext.h>

static inline struct iw_spy_data *get_spydata(struct net_device *dev)
{
	
	if (dev->wireless_data)
		return dev->wireless_data->spy_data;
	return NULL;
}

int iw_handler_set_spy(struct net_device *	dev,
		       struct iw_request_info *	info,
		       union iwreq_data *	wrqu,
		       char *			extra)
{
	struct iw_spy_data *	spydata = get_spydata(dev);
	struct sockaddr *	address = (struct sockaddr *) extra;

	
	if (!spydata)
		return -EOPNOTSUPP;

	spydata->spy_number = 0;

	smp_wmb();

	
	if (wrqu->data.length > 0) {
		int i;

		
		for (i = 0; i < wrqu->data.length; i++)
			memcpy(spydata->spy_address[i], address[i].sa_data,
			       ETH_ALEN);
		
		memset(spydata->spy_stat, 0,
		       sizeof(struct iw_quality) * IW_MAX_SPY);
	}

	
	smp_wmb();

	
	spydata->spy_number = wrqu->data.length;

	return 0;
}
EXPORT_SYMBOL(iw_handler_set_spy);

int iw_handler_get_spy(struct net_device *	dev,
		       struct iw_request_info *	info,
		       union iwreq_data *	wrqu,
		       char *			extra)
{
	struct iw_spy_data *	spydata = get_spydata(dev);
	struct sockaddr *	address = (struct sockaddr *) extra;
	int			i;

	
	if (!spydata)
		return -EOPNOTSUPP;

	wrqu->data.length = spydata->spy_number;

	
	for (i = 0; i < spydata->spy_number; i++) 	{
		memcpy(address[i].sa_data, spydata->spy_address[i], ETH_ALEN);
		address[i].sa_family = AF_UNIX;
	}
	
	if (spydata->spy_number > 0)
		memcpy(extra  + (sizeof(struct sockaddr) *spydata->spy_number),
		       spydata->spy_stat,
		       sizeof(struct iw_quality) * spydata->spy_number);
	
	for (i = 0; i < spydata->spy_number; i++)
		spydata->spy_stat[i].updated &= ~IW_QUAL_ALL_UPDATED;
	return 0;
}
EXPORT_SYMBOL(iw_handler_get_spy);

int iw_handler_set_thrspy(struct net_device *	dev,
			  struct iw_request_info *info,
			  union iwreq_data *	wrqu,
			  char *		extra)
{
	struct iw_spy_data *	spydata = get_spydata(dev);
	struct iw_thrspy *	threshold = (struct iw_thrspy *) extra;

	
	if (!spydata)
		return -EOPNOTSUPP;

	
	memcpy(&(spydata->spy_thr_low), &(threshold->low),
	       2 * sizeof(struct iw_quality));

	
	memset(spydata->spy_thr_under, '\0', sizeof(spydata->spy_thr_under));

	return 0;
}
EXPORT_SYMBOL(iw_handler_set_thrspy);

int iw_handler_get_thrspy(struct net_device *	dev,
			  struct iw_request_info *info,
			  union iwreq_data *	wrqu,
			  char *		extra)
{
	struct iw_spy_data *	spydata = get_spydata(dev);
	struct iw_thrspy *	threshold = (struct iw_thrspy *) extra;

	
	if (!spydata)
		return -EOPNOTSUPP;

	
	memcpy(&(threshold->low), &(spydata->spy_thr_low),
	       2 * sizeof(struct iw_quality));

	return 0;
}
EXPORT_SYMBOL(iw_handler_get_thrspy);

static void iw_send_thrspy_event(struct net_device *	dev,
				 struct iw_spy_data *	spydata,
				 unsigned char *	address,
				 struct iw_quality *	wstats)
{
	union iwreq_data	wrqu;
	struct iw_thrspy	threshold;

	
	wrqu.data.length = 1;
	wrqu.data.flags = 0;
	
	memcpy(threshold.addr.sa_data, address, ETH_ALEN);
	threshold.addr.sa_family = ARPHRD_ETHER;
	
	memcpy(&(threshold.qual), wstats, sizeof(struct iw_quality));
	
	memcpy(&(threshold.low), &(spydata->spy_thr_low),
	       2 * sizeof(struct iw_quality));

	
	wireless_send_event(dev, SIOCGIWTHRSPY, &wrqu, (char *) &threshold);
}

void wireless_spy_update(struct net_device *	dev,
			 unsigned char *	address,
			 struct iw_quality *	wstats)
{
	struct iw_spy_data *	spydata = get_spydata(dev);
	int			i;
	int			match = -1;

	
	if (!spydata)
		return;

	
	for (i = 0; i < spydata->spy_number; i++)
		if (!compare_ether_addr(address, spydata->spy_address[i])) {
			memcpy(&(spydata->spy_stat[i]), wstats,
			       sizeof(struct iw_quality));
			match = i;
		}

	if (match >= 0) {
		if (spydata->spy_thr_under[match]) {
			if (wstats->level > spydata->spy_thr_high.level) {
				spydata->spy_thr_under[match] = 0;
				iw_send_thrspy_event(dev, spydata,
						     address, wstats);
			}
		} else {
			if (wstats->level < spydata->spy_thr_low.level) {
				spydata->spy_thr_under[match] = 1;
				iw_send_thrspy_event(dev, spydata,
						     address, wstats);
			}
		}
	}
}
EXPORT_SYMBOL(wireless_spy_update);
