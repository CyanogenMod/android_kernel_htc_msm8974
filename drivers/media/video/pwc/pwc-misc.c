/* Linux driver for Philips webcam
   Various miscellaneous functions and tables.
   (C) 1999-2003 Nemosoft Unv.
   (C) 2004-2006 Luc Saillard (luc@saillard.org)

   NOTE: this version of pwc is an unofficial (modified) release of pwc & pcwx
   driver and thus may have bugs that are not present in the original version.
   Please send bug reports and support requests to <luc@saillard.org>.
   The decompression routines have been implemented by reverse-engineering the
   Nemosoft binary pwcx module. Caveat emptor.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#include "pwc.h"

const int pwc_image_sizes[PSZ_MAX][2] =
{
	{ 128,  96 }, 
	{ 160, 120 }, 
	{ 176, 144 }, 
	{ 320, 240 }, 
	{ 352, 288 }, 
	{ 640, 480 }, 
};

int pwc_get_size(struct pwc_device *pdev, int width, int height)
{
	int i;

	for (i = PSZ_MAX - 1; i >= 0; i--) {
		if (!(pdev->image_mask & (1 << i)))
			continue;

		if (pwc_image_sizes[i][0] <= width &&
		    pwc_image_sizes[i][1] <= height)
			return i;
	}

	
	for (i = 0; i < PSZ_MAX; i++) {
		if (pdev->image_mask & (1 << i))
			return i;
	}

	
	return 0;
}

void pwc_construct(struct pwc_device *pdev)
{
	if (DEVICE_USE_CODEC1(pdev->type)) {

		pdev->image_mask = 1 << PSZ_SQCIF | 1 << PSZ_QCIF | 1 << PSZ_CIF;
		pdev->vcinterface = 2;
		pdev->vendpoint = 4;
		pdev->frame_header_size = 0;
		pdev->frame_trailer_size = 0;

	} else if (DEVICE_USE_CODEC3(pdev->type)) {

		pdev->image_mask = 1 << PSZ_QSIF | 1 << PSZ_SIF | 1 << PSZ_VGA;
		pdev->vcinterface = 3;
		pdev->vendpoint = 5;
		pdev->frame_header_size = TOUCAM_HEADER_SIZE;
		pdev->frame_trailer_size = TOUCAM_TRAILER_SIZE;

	} else  {

		pdev->image_mask = 1 << PSZ_SQCIF | 1 << PSZ_QSIF | 1 << PSZ_QCIF | 1 << PSZ_SIF | 1 << PSZ_CIF | 1 << PSZ_VGA;
		pdev->vcinterface = 3;
		pdev->vendpoint = 4;
		pdev->frame_header_size = 0;
		pdev->frame_trailer_size = 0;
	}
}
