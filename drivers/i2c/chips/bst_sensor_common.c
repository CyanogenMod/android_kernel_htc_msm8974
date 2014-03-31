/*
 * Created on Sep 19th, 2012, by guz4sgh
 * Last modified: Sep 19th, 2012
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2012 Bosch Sensortec GmbH
 * All Rights Reserved
 */

#include <linux/bst_sensor_common.h>

#define MAX_AXIS_REMAP_TAB_SZ 8    
const static bosch_sensor_axis_remap
bst_axis_remap_tab_dft[MAX_AXIS_REMAP_TAB_SZ] = {
	
	{  0,    1,    2,     1,      1,      1 }, 
	{  1,    0,    2,     1,     -1,      1 }, 
	{  0,    1,    2,    -1,     -1,      1 }, 
	{  1,    0,    2,    -1,      1,      1 }, 

	{  0,    1,    2,    -1,      1,     -1 }, 
	{  1,    0,    2,    -1,     -1,     -1 }, 
	{  0,    1,    2,     1,     -1,     -1 }, 
	{  1,    0,    2,     1,      1,     -1 }, 
};


void bst_remap_sensor_data(struct bosch_sensor_data *data,
		const bosch_sensor_axis_remap *remap)
{
	struct bosch_sensor_data tmp;

	tmp.x = data->v[remap->src_x] * remap->sign_x;
	tmp.y = data->v[remap->src_y] * remap->sign_y;
	tmp.z = data->v[remap->src_z] * remap->sign_z;

	memcpy(data, &tmp, sizeof(*data));
}


void bst_remap_sensor_data_dft_tab(struct bosch_sensor_data *data,
		int place)
{
	if (0 != place) {
		bst_remap_sensor_data(data, &bst_axis_remap_tab_dft[place]);
	}
}
