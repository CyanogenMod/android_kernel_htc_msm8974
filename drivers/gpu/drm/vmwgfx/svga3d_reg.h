/**********************************************************
 * Copyright 1998-2009 VMware, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************/


#ifndef _SVGA3D_REG_H_
#define _SVGA3D_REG_H_

#include "svga_reg.h"



#define SVGA3D_MAKE_HWVERSION(major, minor)      (((major) << 16) | ((minor) & 0xFF))
#define SVGA3D_MAJOR_HWVERSION(version)          ((version) >> 16)
#define SVGA3D_MINOR_HWVERSION(version)          ((version) & 0xFF)

typedef enum {
   SVGA3D_HWVERSION_WS5_RC1   = SVGA3D_MAKE_HWVERSION(0, 1),
   SVGA3D_HWVERSION_WS5_RC2   = SVGA3D_MAKE_HWVERSION(0, 2),
   SVGA3D_HWVERSION_WS51_RC1  = SVGA3D_MAKE_HWVERSION(0, 3),
   SVGA3D_HWVERSION_WS6_B1    = SVGA3D_MAKE_HWVERSION(1, 1),
   SVGA3D_HWVERSION_FUSION_11 = SVGA3D_MAKE_HWVERSION(1, 4),
   SVGA3D_HWVERSION_WS65_B1   = SVGA3D_MAKE_HWVERSION(2, 0),
   SVGA3D_HWVERSION_WS8_B1    = SVGA3D_MAKE_HWVERSION(2, 1),
   SVGA3D_HWVERSION_CURRENT   = SVGA3D_HWVERSION_WS8_B1,
} SVGA3dHardwareVersion;


typedef uint32 SVGA3dBool; 
#define SVGA3D_NUM_CLIPPLANES                   6
#define SVGA3D_MAX_SIMULTANEOUS_RENDER_TARGETS  8
#define SVGA3D_MAX_CONTEXT_IDS                  256
#define SVGA3D_MAX_SURFACE_IDS                  (32 * 1024)


typedef enum SVGA3dSurfaceFormat {
   SVGA3D_FORMAT_INVALID               = 0,

   SVGA3D_X8R8G8B8                     = 1,
   SVGA3D_A8R8G8B8                     = 2,

   SVGA3D_R5G6B5                       = 3,
   SVGA3D_X1R5G5B5                     = 4,
   SVGA3D_A1R5G5B5                     = 5,
   SVGA3D_A4R4G4B4                     = 6,

   SVGA3D_Z_D32                        = 7,
   SVGA3D_Z_D16                        = 8,
   SVGA3D_Z_D24S8                      = 9,
   SVGA3D_Z_D15S1                      = 10,

   SVGA3D_LUMINANCE8                   = 11,
   SVGA3D_LUMINANCE4_ALPHA4            = 12,
   SVGA3D_LUMINANCE16                  = 13,
   SVGA3D_LUMINANCE8_ALPHA8            = 14,

   SVGA3D_DXT1                         = 15,
   SVGA3D_DXT2                         = 16,
   SVGA3D_DXT3                         = 17,
   SVGA3D_DXT4                         = 18,
   SVGA3D_DXT5                         = 19,

   SVGA3D_BUMPU8V8                     = 20,
   SVGA3D_BUMPL6V5U5                   = 21,
   SVGA3D_BUMPX8L8V8U8                 = 22,
   SVGA3D_BUMPL8V8U8                   = 23,

   SVGA3D_ARGB_S10E5                   = 24,   
   SVGA3D_ARGB_S23E8                   = 25,   

   SVGA3D_A2R10G10B10                  = 26,

   
   SVGA3D_V8U8                         = 27,
   SVGA3D_Q8W8V8U8                     = 28,
   SVGA3D_CxV8U8                       = 29,

   
   SVGA3D_X8L8V8U8                     = 30,
   SVGA3D_A2W10V10U10                  = 31,

   SVGA3D_ALPHA8                       = 32,

   
   SVGA3D_R_S10E5                      = 33,
   SVGA3D_R_S23E8                      = 34,
   SVGA3D_RG_S10E5                     = 35,
   SVGA3D_RG_S23E8                     = 36,


   SVGA3D_BUFFER                       = 37,

   SVGA3D_Z_D24X8                      = 38,

   SVGA3D_V16U16                       = 39,

   SVGA3D_G16R16                       = 40,
   SVGA3D_A16B16G16R16                 = 41,

   
   SVGA3D_UYVY                         = 42,
   SVGA3D_YUY2                         = 43,

   
   SVGA3D_NV12                         = 44,

   
   SVGA3D_AYUV                         = 45,

   SVGA3D_BC4_UNORM                    = 108,
   SVGA3D_BC5_UNORM                    = 111,

   
   SVGA3D_Z_DF16                       = 118,
   SVGA3D_Z_DF24                       = 119,
   SVGA3D_Z_D24S8_INT                  = 120,

   SVGA3D_FORMAT_MAX
} SVGA3dSurfaceFormat;

typedef uint32 SVGA3dColor; 

typedef enum {
   SVGA3DFORMAT_OP_TEXTURE                               = 0x00000001,
   SVGA3DFORMAT_OP_VOLUMETEXTURE                         = 0x00000002,
   SVGA3DFORMAT_OP_CUBETEXTURE                           = 0x00000004,
   SVGA3DFORMAT_OP_OFFSCREEN_RENDERTARGET                = 0x00000008,
   SVGA3DFORMAT_OP_SAME_FORMAT_RENDERTARGET              = 0x00000010,
   SVGA3DFORMAT_OP_ZSTENCIL                              = 0x00000040,
   SVGA3DFORMAT_OP_ZSTENCIL_WITH_ARBITRARY_COLOR_DEPTH   = 0x00000080,

   SVGA3DFORMAT_OP_SAME_FORMAT_UP_TO_ALPHA_RENDERTARGET  = 0x00000100,

   SVGA3DFORMAT_OP_DISPLAYMODE                           = 0x00000400,

   SVGA3DFORMAT_OP_3DACCELERATION                        = 0x00000800,

   SVGA3DFORMAT_OP_PIXELSIZE                             = 0x00001000,

   SVGA3DFORMAT_OP_CONVERT_TO_ARGB                       = 0x00002000,

   SVGA3DFORMAT_OP_OFFSCREENPLAIN                        = 0x00004000,

   SVGA3DFORMAT_OP_SRGBREAD                              = 0x00008000,

   SVGA3DFORMAT_OP_BUMPMAP                               = 0x00010000,

   SVGA3DFORMAT_OP_DMAP                                  = 0x00020000,

   SVGA3DFORMAT_OP_NOFILTER                              = 0x00040000,

   SVGA3DFORMAT_OP_MEMBEROFGROUP_ARGB                    = 0x00080000,

/*
 * Indicated that this format can be written as an SRGB target (meaning that the
 * pixel pipe will DE-linearize data on output to format)
 */
   SVGA3DFORMAT_OP_SRGBWRITE                             = 0x00100000,

   SVGA3DFORMAT_OP_NOALPHABLEND                          = 0x00200000,

   SVGA3DFORMAT_OP_AUTOGENMIPMAP                         = 0x00400000,

   SVGA3DFORMAT_OP_VERTEXTEXTURE                         = 0x00800000,

   SVGA3DFORMAT_OP_NOTEXCOORDWRAPNORMIP                  = 0x01000000
} SVGA3dFormatOp;

typedef union {
   uint32 value;
   struct {
      uint32 texture : 1;
      uint32 volumeTexture : 1;
      uint32 cubeTexture : 1;
      uint32 offscreenRenderTarget : 1;
      uint32 sameFormatRenderTarget : 1;
      uint32 unknown1 : 1;
      uint32 zStencil : 1;
      uint32 zStencilArbitraryDepth : 1;
      uint32 sameFormatUpToAlpha : 1;
      uint32 unknown2 : 1;
      uint32 displayMode : 1;
      uint32 acceleration3d : 1;
      uint32 pixelSize : 1;
      uint32 convertToARGB : 1;
      uint32 offscreenPlain : 1;
      uint32 sRGBRead : 1;
      uint32 bumpMap : 1;
      uint32 dmap : 1;
      uint32 noFilter : 1;
      uint32 memberOfGroupARGB : 1;
      uint32 sRGBWrite : 1;
      uint32 noAlphaBlend : 1;
      uint32 autoGenMipMap : 1;
      uint32 vertexTexture : 1;
      uint32 noTexCoordWrapNorMip : 1;
   };
} SVGA3dSurfaceFormatCaps;


typedef enum {
   SVGA3D_RS_INVALID                   = 0,
   SVGA3D_RS_ZENABLE                   = 1,     
   SVGA3D_RS_ZWRITEENABLE              = 2,     
   SVGA3D_RS_ALPHATESTENABLE           = 3,     
   SVGA3D_RS_DITHERENABLE              = 4,     
   SVGA3D_RS_BLENDENABLE               = 5,     
   SVGA3D_RS_FOGENABLE                 = 6,     
   SVGA3D_RS_SPECULARENABLE            = 7,     
   SVGA3D_RS_STENCILENABLE             = 8,     
   SVGA3D_RS_LIGHTINGENABLE            = 9,     
   SVGA3D_RS_NORMALIZENORMALS          = 10,    
   SVGA3D_RS_POINTSPRITEENABLE         = 11,    
   SVGA3D_RS_POINTSCALEENABLE          = 12,    
   SVGA3D_RS_STENCILREF                = 13,    
   SVGA3D_RS_STENCILMASK               = 14,    
   SVGA3D_RS_STENCILWRITEMASK          = 15,    
   SVGA3D_RS_FOGSTART                  = 16,    
   SVGA3D_RS_FOGEND                    = 17,    
   SVGA3D_RS_FOGDENSITY                = 18,    
   SVGA3D_RS_POINTSIZE                 = 19,    
   SVGA3D_RS_POINTSIZEMIN              = 20,    
   SVGA3D_RS_POINTSIZEMAX              = 21,    
   SVGA3D_RS_POINTSCALE_A              = 22,    
   SVGA3D_RS_POINTSCALE_B              = 23,    
   SVGA3D_RS_POINTSCALE_C              = 24,    
   SVGA3D_RS_FOGCOLOR                  = 25,    
   SVGA3D_RS_AMBIENT                   = 26,    
   SVGA3D_RS_CLIPPLANEENABLE           = 27,    
   SVGA3D_RS_FOGMODE                   = 28,    
   SVGA3D_RS_FILLMODE                  = 29,    
   SVGA3D_RS_SHADEMODE                 = 30,    
   SVGA3D_RS_LINEPATTERN               = 31,    
   SVGA3D_RS_SRCBLEND                  = 32,    
   SVGA3D_RS_DSTBLEND                  = 33,    
   SVGA3D_RS_BLENDEQUATION             = 34,    
   SVGA3D_RS_CULLMODE                  = 35,    
   SVGA3D_RS_ZFUNC                     = 36,    
   SVGA3D_RS_ALPHAFUNC                 = 37,    
   SVGA3D_RS_STENCILFUNC               = 38,    
   SVGA3D_RS_STENCILFAIL               = 39,    
   SVGA3D_RS_STENCILZFAIL              = 40,    
   SVGA3D_RS_STENCILPASS               = 41,    
   SVGA3D_RS_ALPHAREF                  = 42,    
   SVGA3D_RS_FRONTWINDING              = 43,    
   SVGA3D_RS_COORDINATETYPE            = 44,    
   SVGA3D_RS_ZBIAS                     = 45,    
   SVGA3D_RS_RANGEFOGENABLE            = 46,    
   SVGA3D_RS_COLORWRITEENABLE          = 47,    
   SVGA3D_RS_VERTEXMATERIALENABLE      = 48,    
   SVGA3D_RS_DIFFUSEMATERIALSOURCE     = 49,    
   SVGA3D_RS_SPECULARMATERIALSOURCE    = 50,    
   SVGA3D_RS_AMBIENTMATERIALSOURCE     = 51,    
   SVGA3D_RS_EMISSIVEMATERIALSOURCE    = 52,    
   SVGA3D_RS_TEXTUREFACTOR             = 53,    
   SVGA3D_RS_LOCALVIEWER               = 54,    
   SVGA3D_RS_SCISSORTESTENABLE         = 55,    
   SVGA3D_RS_BLENDCOLOR                = 56,    
   SVGA3D_RS_STENCILENABLE2SIDED       = 57,    
   SVGA3D_RS_CCWSTENCILFUNC            = 58,    
   SVGA3D_RS_CCWSTENCILFAIL            = 59,    
   SVGA3D_RS_CCWSTENCILZFAIL           = 60,    
   SVGA3D_RS_CCWSTENCILPASS            = 61,    
   SVGA3D_RS_VERTEXBLEND               = 62,    
   SVGA3D_RS_SLOPESCALEDEPTHBIAS       = 63,    
   SVGA3D_RS_DEPTHBIAS                 = 64,    



   SVGA3D_RS_OUTPUTGAMMA               = 65,    
   SVGA3D_RS_ZVISIBLE                  = 66,    
   SVGA3D_RS_LASTPIXEL                 = 67,    
   SVGA3D_RS_CLIPPING                  = 68,    
   SVGA3D_RS_WRAP0                     = 69,    
   SVGA3D_RS_WRAP1                     = 70,    
   SVGA3D_RS_WRAP2                     = 71,    
   SVGA3D_RS_WRAP3                     = 72,    
   SVGA3D_RS_WRAP4                     = 73,    
   SVGA3D_RS_WRAP5                     = 74,    
   SVGA3D_RS_WRAP6                     = 75,    
   SVGA3D_RS_WRAP7                     = 76,    
   SVGA3D_RS_WRAP8                     = 77,    
   SVGA3D_RS_WRAP9                     = 78,    
   SVGA3D_RS_WRAP10                    = 79,    
   SVGA3D_RS_WRAP11                    = 80,    
   SVGA3D_RS_WRAP12                    = 81,    
   SVGA3D_RS_WRAP13                    = 82,    
   SVGA3D_RS_WRAP14                    = 83,    
   SVGA3D_RS_WRAP15                    = 84,    
   SVGA3D_RS_MULTISAMPLEANTIALIAS      = 85,    
   SVGA3D_RS_MULTISAMPLEMASK           = 86,    
   SVGA3D_RS_INDEXEDVERTEXBLENDENABLE  = 87,    
   SVGA3D_RS_TWEENFACTOR               = 88,    
   SVGA3D_RS_ANTIALIASEDLINEENABLE     = 89,    
   SVGA3D_RS_COLORWRITEENABLE1         = 90,    
   SVGA3D_RS_COLORWRITEENABLE2         = 91,    
   SVGA3D_RS_COLORWRITEENABLE3         = 92,    
   SVGA3D_RS_SEPARATEALPHABLENDENABLE  = 93,    
   SVGA3D_RS_SRCBLENDALPHA             = 94,    
   SVGA3D_RS_DSTBLENDALPHA             = 95,    
   SVGA3D_RS_BLENDEQUATIONALPHA        = 96,    
   SVGA3D_RS_TRANSPARENCYANTIALIAS     = 97,    
   SVGA3D_RS_LINEAA                    = 98,    
   SVGA3D_RS_LINEWIDTH                 = 99,    
   SVGA3D_RS_MAX
} SVGA3dRenderStateName;

typedef enum {
   SVGA3D_TRANSPARENCYANTIALIAS_NORMAL            = 0,
   SVGA3D_TRANSPARENCYANTIALIAS_ALPHATOCOVERAGE   = 1,
   SVGA3D_TRANSPARENCYANTIALIAS_SUPERSAMPLE       = 2,
   SVGA3D_TRANSPARENCYANTIALIAS_MAX
} SVGA3dTransparencyAntialiasType;

typedef enum {
   SVGA3D_VERTEXMATERIAL_NONE     = 0,    
   SVGA3D_VERTEXMATERIAL_DIFFUSE  = 1,    
   SVGA3D_VERTEXMATERIAL_SPECULAR = 2,    
} SVGA3dVertexMaterial;

typedef enum {
   SVGA3D_FILLMODE_INVALID = 0,
   SVGA3D_FILLMODE_POINT   = 1,
   SVGA3D_FILLMODE_LINE    = 2,
   SVGA3D_FILLMODE_FILL    = 3,
   SVGA3D_FILLMODE_MAX
} SVGA3dFillModeType;


typedef
union {
   struct {
      uint16   mode;       
      uint16   face;       
   };
   uint32 uintValue;
} SVGA3dFillMode;

typedef enum {
   SVGA3D_SHADEMODE_INVALID = 0,
   SVGA3D_SHADEMODE_FLAT    = 1,
   SVGA3D_SHADEMODE_SMOOTH  = 2,
   SVGA3D_SHADEMODE_PHONG   = 3,     
   SVGA3D_SHADEMODE_MAX
} SVGA3dShadeMode;

typedef
union {
   struct {
      uint16 repeat;
      uint16 pattern;
   };
   uint32 uintValue;
} SVGA3dLinePattern;

typedef enum {
   SVGA3D_BLENDOP_INVALID            = 0,
   SVGA3D_BLENDOP_ZERO               = 1,
   SVGA3D_BLENDOP_ONE                = 2,
   SVGA3D_BLENDOP_SRCCOLOR           = 3,
   SVGA3D_BLENDOP_INVSRCCOLOR        = 4,
   SVGA3D_BLENDOP_SRCALPHA           = 5,
   SVGA3D_BLENDOP_INVSRCALPHA        = 6,
   SVGA3D_BLENDOP_DESTALPHA          = 7,
   SVGA3D_BLENDOP_INVDESTALPHA       = 8,
   SVGA3D_BLENDOP_DESTCOLOR          = 9,
   SVGA3D_BLENDOP_INVDESTCOLOR       = 10,
   SVGA3D_BLENDOP_SRCALPHASAT        = 11,
   SVGA3D_BLENDOP_BLENDFACTOR        = 12,
   SVGA3D_BLENDOP_INVBLENDFACTOR     = 13,
   SVGA3D_BLENDOP_MAX
} SVGA3dBlendOp;

typedef enum {
   SVGA3D_BLENDEQ_INVALID            = 0,
   SVGA3D_BLENDEQ_ADD                = 1,
   SVGA3D_BLENDEQ_SUBTRACT           = 2,
   SVGA3D_BLENDEQ_REVSUBTRACT        = 3,
   SVGA3D_BLENDEQ_MINIMUM            = 4,
   SVGA3D_BLENDEQ_MAXIMUM            = 5,
   SVGA3D_BLENDEQ_MAX
} SVGA3dBlendEquation;

typedef enum {
   SVGA3D_FRONTWINDING_INVALID = 0,
   SVGA3D_FRONTWINDING_CW      = 1,
   SVGA3D_FRONTWINDING_CCW     = 2,
   SVGA3D_FRONTWINDING_MAX
} SVGA3dFrontWinding;

typedef enum {
   SVGA3D_FACE_INVALID  = 0,
   SVGA3D_FACE_NONE     = 1,
   SVGA3D_FACE_FRONT    = 2,
   SVGA3D_FACE_BACK     = 3,
   SVGA3D_FACE_FRONT_BACK = 4,
   SVGA3D_FACE_MAX
} SVGA3dFace;


typedef enum {
   SVGA3D_CMP_INVALID              = 0,
   SVGA3D_CMP_NEVER                = 1,
   SVGA3D_CMP_LESS                 = 2,
   SVGA3D_CMP_EQUAL                = 3,
   SVGA3D_CMP_LESSEQUAL            = 4,
   SVGA3D_CMP_GREATER              = 5,
   SVGA3D_CMP_NOTEQUAL             = 6,
   SVGA3D_CMP_GREATEREQUAL         = 7,
   SVGA3D_CMP_ALWAYS               = 8,
   SVGA3D_CMP_MAX
} SVGA3dCmpFunc;

typedef enum {
   SVGA3D_FOGFUNC_INVALID          = 0,
   SVGA3D_FOGFUNC_EXP              = 1,
   SVGA3D_FOGFUNC_EXP2             = 2,
   SVGA3D_FOGFUNC_LINEAR           = 3,
   SVGA3D_FOGFUNC_PER_VERTEX       = 4
} SVGA3dFogFunction;

typedef enum {
   SVGA3D_FOGTYPE_INVALID          = 0,
   SVGA3D_FOGTYPE_VERTEX           = 1,
   SVGA3D_FOGTYPE_PIXEL            = 2,
   SVGA3D_FOGTYPE_MAX              = 3
} SVGA3dFogType;

typedef enum {
   SVGA3D_FOGBASE_INVALID          = 0,
   SVGA3D_FOGBASE_DEPTHBASED       = 1,
   SVGA3D_FOGBASE_RANGEBASED       = 2,
   SVGA3D_FOGBASE_MAX              = 3
} SVGA3dFogBase;

typedef enum {
   SVGA3D_STENCILOP_INVALID        = 0,
   SVGA3D_STENCILOP_KEEP           = 1,
   SVGA3D_STENCILOP_ZERO           = 2,
   SVGA3D_STENCILOP_REPLACE        = 3,
   SVGA3D_STENCILOP_INCRSAT        = 4,
   SVGA3D_STENCILOP_DECRSAT        = 5,
   SVGA3D_STENCILOP_INVERT         = 6,
   SVGA3D_STENCILOP_INCR           = 7,
   SVGA3D_STENCILOP_DECR           = 8,
   SVGA3D_STENCILOP_MAX
} SVGA3dStencilOp;

typedef enum {
   SVGA3D_CLIPPLANE_0              = (1 << 0),
   SVGA3D_CLIPPLANE_1              = (1 << 1),
   SVGA3D_CLIPPLANE_2              = (1 << 2),
   SVGA3D_CLIPPLANE_3              = (1 << 3),
   SVGA3D_CLIPPLANE_4              = (1 << 4),
   SVGA3D_CLIPPLANE_5              = (1 << 5),
} SVGA3dClipPlanes;

typedef enum {
   SVGA3D_CLEAR_COLOR              = 0x1,
   SVGA3D_CLEAR_DEPTH              = 0x2,
   SVGA3D_CLEAR_STENCIL            = 0x4
} SVGA3dClearFlag;

typedef enum {
   SVGA3D_RT_DEPTH                 = 0,
   SVGA3D_RT_STENCIL               = 1,
   SVGA3D_RT_COLOR0                = 2,
   SVGA3D_RT_COLOR1                = 3,
   SVGA3D_RT_COLOR2                = 4,
   SVGA3D_RT_COLOR3                = 5,
   SVGA3D_RT_COLOR4                = 6,
   SVGA3D_RT_COLOR5                = 7,
   SVGA3D_RT_COLOR6                = 8,
   SVGA3D_RT_COLOR7                = 9,
   SVGA3D_RT_MAX,
   SVGA3D_RT_INVALID               = ((uint32)-1),
} SVGA3dRenderTargetType;

#define SVGA3D_MAX_RT_COLOR (SVGA3D_RT_COLOR7 - SVGA3D_RT_COLOR0 + 1)

typedef
union {
   struct {
      uint32  red   : 1;
      uint32  green : 1;
      uint32  blue  : 1;
      uint32  alpha : 1;
   };
   uint32 uintValue;
} SVGA3dColorMask;

typedef enum {
   SVGA3D_VBLEND_DISABLE            = 0,
   SVGA3D_VBLEND_1WEIGHT            = 1,
   SVGA3D_VBLEND_2WEIGHT            = 2,
   SVGA3D_VBLEND_3WEIGHT            = 3,
} SVGA3dVertexBlendFlags;

typedef enum {
   SVGA3D_WRAPCOORD_0   = 1 << 0,
   SVGA3D_WRAPCOORD_1   = 1 << 1,
   SVGA3D_WRAPCOORD_2   = 1 << 2,
   SVGA3D_WRAPCOORD_3   = 1 << 3,
   SVGA3D_WRAPCOORD_ALL = 0xF,
} SVGA3dWrapFlags;


typedef enum {
   SVGA3D_TS_INVALID                    = 0,
   SVGA3D_TS_BIND_TEXTURE               = 1,    
   SVGA3D_TS_COLOROP                    = 2,    
   SVGA3D_TS_COLORARG1                  = 3,    
   SVGA3D_TS_COLORARG2                  = 4,    
   SVGA3D_TS_ALPHAOP                    = 5,    
   SVGA3D_TS_ALPHAARG1                  = 6,    
   SVGA3D_TS_ALPHAARG2                  = 7,    
   SVGA3D_TS_ADDRESSU                   = 8,    
   SVGA3D_TS_ADDRESSV                   = 9,    
   SVGA3D_TS_MIPFILTER                  = 10,   
   SVGA3D_TS_MAGFILTER                  = 11,   
   SVGA3D_TS_MINFILTER                  = 12,   
   SVGA3D_TS_BORDERCOLOR                = 13,   
   SVGA3D_TS_TEXCOORDINDEX              = 14,   
   SVGA3D_TS_TEXTURETRANSFORMFLAGS      = 15,   
   SVGA3D_TS_TEXCOORDGEN                = 16,   
   SVGA3D_TS_BUMPENVMAT00               = 17,   
   SVGA3D_TS_BUMPENVMAT01               = 18,   
   SVGA3D_TS_BUMPENVMAT10               = 19,   
   SVGA3D_TS_BUMPENVMAT11               = 20,   
   SVGA3D_TS_TEXTURE_MIPMAP_LEVEL       = 21,   
   SVGA3D_TS_TEXTURE_LOD_BIAS           = 22,   
   SVGA3D_TS_TEXTURE_ANISOTROPIC_LEVEL  = 23,   
   SVGA3D_TS_ADDRESSW                   = 24,   



   SVGA3D_TS_GAMMA                      = 25,   
   SVGA3D_TS_BUMPENVLSCALE              = 26,   
   SVGA3D_TS_BUMPENVLOFFSET             = 27,   
   SVGA3D_TS_COLORARG0                  = 28,   
   SVGA3D_TS_ALPHAARG0                  = 29,   
   SVGA3D_TS_MAX
} SVGA3dTextureStateName;

typedef enum {
   SVGA3D_TC_INVALID                   = 0,
   SVGA3D_TC_DISABLE                   = 1,
   SVGA3D_TC_SELECTARG1                = 2,
   SVGA3D_TC_SELECTARG2                = 3,
   SVGA3D_TC_MODULATE                  = 4,
   SVGA3D_TC_ADD                       = 5,
   SVGA3D_TC_ADDSIGNED                 = 6,
   SVGA3D_TC_SUBTRACT                  = 7,
   SVGA3D_TC_BLENDTEXTUREALPHA         = 8,
   SVGA3D_TC_BLENDDIFFUSEALPHA         = 9,
   SVGA3D_TC_BLENDCURRENTALPHA         = 10,
   SVGA3D_TC_BLENDFACTORALPHA          = 11,
   SVGA3D_TC_MODULATE2X                = 12,
   SVGA3D_TC_MODULATE4X                = 13,
   SVGA3D_TC_DSDT                      = 14,
   SVGA3D_TC_DOTPRODUCT3               = 15,
   SVGA3D_TC_BLENDTEXTUREALPHAPM       = 16,
   SVGA3D_TC_ADDSIGNED2X               = 17,
   SVGA3D_TC_ADDSMOOTH                 = 18,
   SVGA3D_TC_PREMODULATE               = 19,
   SVGA3D_TC_MODULATEALPHA_ADDCOLOR    = 20,
   SVGA3D_TC_MODULATECOLOR_ADDALPHA    = 21,
   SVGA3D_TC_MODULATEINVALPHA_ADDCOLOR = 22,
   SVGA3D_TC_MODULATEINVCOLOR_ADDALPHA = 23,
   SVGA3D_TC_BUMPENVMAPLUMINANCE       = 24,
   SVGA3D_TC_MULTIPLYADD               = 25,
   SVGA3D_TC_LERP                      = 26,
   SVGA3D_TC_MAX
} SVGA3dTextureCombiner;

#define SVGA3D_TC_CAP_BIT(svga3d_tc_op) (svga3d_tc_op ? (1 << (svga3d_tc_op - 1)) : 0)

typedef enum {
   SVGA3D_TEX_ADDRESS_INVALID    = 0,
   SVGA3D_TEX_ADDRESS_WRAP       = 1,
   SVGA3D_TEX_ADDRESS_MIRROR     = 2,
   SVGA3D_TEX_ADDRESS_CLAMP      = 3,
   SVGA3D_TEX_ADDRESS_BORDER     = 4,
   SVGA3D_TEX_ADDRESS_MIRRORONCE = 5,
   SVGA3D_TEX_ADDRESS_EDGE       = 6,
   SVGA3D_TEX_ADDRESS_MAX
} SVGA3dTextureAddress;

typedef enum {
   SVGA3D_TEX_FILTER_NONE           = 0,
   SVGA3D_TEX_FILTER_NEAREST        = 1,
   SVGA3D_TEX_FILTER_LINEAR         = 2,
   SVGA3D_TEX_FILTER_ANISOTROPIC    = 3,
   SVGA3D_TEX_FILTER_FLATCUBIC      = 4, 
   SVGA3D_TEX_FILTER_GAUSSIANCUBIC  = 5, 
   SVGA3D_TEX_FILTER_PYRAMIDALQUAD  = 6, 
   SVGA3D_TEX_FILTER_GAUSSIANQUAD   = 7, 
   SVGA3D_TEX_FILTER_MAX
} SVGA3dTextureFilter;

typedef enum {
   SVGA3D_TEX_TRANSFORM_OFF    = 0,
   SVGA3D_TEX_TRANSFORM_S      = (1 << 0),
   SVGA3D_TEX_TRANSFORM_T      = (1 << 1),
   SVGA3D_TEX_TRANSFORM_R      = (1 << 2),
   SVGA3D_TEX_TRANSFORM_Q      = (1 << 3),
   SVGA3D_TEX_PROJECTED        = (1 << 15),
} SVGA3dTexTransformFlags;

typedef enum {
   SVGA3D_TEXCOORD_GEN_OFF              = 0,
   SVGA3D_TEXCOORD_GEN_EYE_POSITION     = 1,
   SVGA3D_TEXCOORD_GEN_EYE_NORMAL       = 2,
   SVGA3D_TEXCOORD_GEN_REFLECTIONVECTOR = 3,
   SVGA3D_TEXCOORD_GEN_SPHERE           = 4,
   SVGA3D_TEXCOORD_GEN_MAX
} SVGA3dTextureCoordGen;

typedef enum {
   SVGA3D_TA_INVALID    = 0,
   SVGA3D_TA_CONSTANT   = 1,
   SVGA3D_TA_PREVIOUS   = 2,
   SVGA3D_TA_DIFFUSE    = 3,
   SVGA3D_TA_TEXTURE    = 4,
   SVGA3D_TA_SPECULAR   = 5,
   SVGA3D_TA_MAX
} SVGA3dTextureArgData;

#define SVGA3D_TM_MASK_LEN 4

typedef enum {
   SVGA3D_TM_NONE       = 0,
   SVGA3D_TM_ALPHA      = (1 << SVGA3D_TM_MASK_LEN),
   SVGA3D_TM_ONE_MINUS  = (2 << SVGA3D_TM_MASK_LEN),
} SVGA3dTextureArgModifier;

#define SVGA3D_INVALID_ID         ((uint32)-1)
#define SVGA3D_MAX_CLIP_PLANES    6

#define SVGA3D_MAX_TEXTURE_COORDS 8


typedef enum {
   SVGA3D_DECLUSAGE_POSITION     = 0,
   SVGA3D_DECLUSAGE_BLENDWEIGHT,       
   SVGA3D_DECLUSAGE_BLENDINDICES,      
   SVGA3D_DECLUSAGE_NORMAL,            
   SVGA3D_DECLUSAGE_PSIZE,             
   SVGA3D_DECLUSAGE_TEXCOORD,          
   SVGA3D_DECLUSAGE_TANGENT,           
   SVGA3D_DECLUSAGE_BINORMAL,          
   SVGA3D_DECLUSAGE_TESSFACTOR,        
   SVGA3D_DECLUSAGE_POSITIONT,         
   SVGA3D_DECLUSAGE_COLOR,             
   SVGA3D_DECLUSAGE_FOG,               
   SVGA3D_DECLUSAGE_DEPTH,             
   SVGA3D_DECLUSAGE_SAMPLE,            
   SVGA3D_DECLUSAGE_MAX
} SVGA3dDeclUsage;

typedef enum {
   SVGA3D_DECLMETHOD_DEFAULT     = 0,
   SVGA3D_DECLMETHOD_PARTIALU,
   SVGA3D_DECLMETHOD_PARTIALV,
   SVGA3D_DECLMETHOD_CROSSUV,          
   SVGA3D_DECLMETHOD_UV,
   SVGA3D_DECLMETHOD_LOOKUP,           
   SVGA3D_DECLMETHOD_LOOKUPPRESAMPLED, 
} SVGA3dDeclMethod;

typedef enum {
   SVGA3D_DECLTYPE_FLOAT1        =  0,
   SVGA3D_DECLTYPE_FLOAT2        =  1,
   SVGA3D_DECLTYPE_FLOAT3        =  2,
   SVGA3D_DECLTYPE_FLOAT4        =  3,
   SVGA3D_DECLTYPE_D3DCOLOR      =  4,
   SVGA3D_DECLTYPE_UBYTE4        =  5,
   SVGA3D_DECLTYPE_SHORT2        =  6,
   SVGA3D_DECLTYPE_SHORT4        =  7,
   SVGA3D_DECLTYPE_UBYTE4N       =  8,
   SVGA3D_DECLTYPE_SHORT2N       =  9,
   SVGA3D_DECLTYPE_SHORT4N       = 10,
   SVGA3D_DECLTYPE_USHORT2N      = 11,
   SVGA3D_DECLTYPE_USHORT4N      = 12,
   SVGA3D_DECLTYPE_UDEC3         = 13,
   SVGA3D_DECLTYPE_DEC3N         = 14,
   SVGA3D_DECLTYPE_FLOAT16_2     = 15,
   SVGA3D_DECLTYPE_FLOAT16_4     = 16,
   SVGA3D_DECLTYPE_MAX,
} SVGA3dDeclType;

typedef union {
   struct {
      uint32 count : 30;

      uint32 indexedData : 1;

      uint32 instanceData : 1;
   };

   uint32 value;
} SVGA3dVertexDivisor;

typedef enum {
   SVGA3D_PRIMITIVE_INVALID                     = 0,
   SVGA3D_PRIMITIVE_TRIANGLELIST                = 1,
   SVGA3D_PRIMITIVE_POINTLIST                   = 2,
   SVGA3D_PRIMITIVE_LINELIST                    = 3,
   SVGA3D_PRIMITIVE_LINESTRIP                   = 4,
   SVGA3D_PRIMITIVE_TRIANGLESTRIP               = 5,
   SVGA3D_PRIMITIVE_TRIANGLEFAN                 = 6,
   SVGA3D_PRIMITIVE_MAX
} SVGA3dPrimitiveType;

typedef enum {
   SVGA3D_COORDINATE_INVALID                   = 0,
   SVGA3D_COORDINATE_LEFTHANDED                = 1,
   SVGA3D_COORDINATE_RIGHTHANDED               = 2,
   SVGA3D_COORDINATE_MAX
} SVGA3dCoordinateType;

typedef enum {
   SVGA3D_TRANSFORM_INVALID                     = 0,
   SVGA3D_TRANSFORM_WORLD                       = 1,
   SVGA3D_TRANSFORM_VIEW                        = 2,
   SVGA3D_TRANSFORM_PROJECTION                  = 3,
   SVGA3D_TRANSFORM_TEXTURE0                    = 4,
   SVGA3D_TRANSFORM_TEXTURE1                    = 5,
   SVGA3D_TRANSFORM_TEXTURE2                    = 6,
   SVGA3D_TRANSFORM_TEXTURE3                    = 7,
   SVGA3D_TRANSFORM_TEXTURE4                    = 8,
   SVGA3D_TRANSFORM_TEXTURE5                    = 9,
   SVGA3D_TRANSFORM_TEXTURE6                    = 10,
   SVGA3D_TRANSFORM_TEXTURE7                    = 11,
   SVGA3D_TRANSFORM_WORLD1                      = 12,
   SVGA3D_TRANSFORM_WORLD2                      = 13,
   SVGA3D_TRANSFORM_WORLD3                      = 14,
   SVGA3D_TRANSFORM_MAX
} SVGA3dTransformType;

typedef enum {
   SVGA3D_LIGHTTYPE_INVALID                     = 0,
   SVGA3D_LIGHTTYPE_POINT                       = 1,
   SVGA3D_LIGHTTYPE_SPOT1                       = 2, 
   SVGA3D_LIGHTTYPE_SPOT2                       = 3, 
   SVGA3D_LIGHTTYPE_DIRECTIONAL                 = 4,
   SVGA3D_LIGHTTYPE_MAX
} SVGA3dLightType;

typedef enum {
   SVGA3D_CUBEFACE_POSX                         = 0,
   SVGA3D_CUBEFACE_NEGX                         = 1,
   SVGA3D_CUBEFACE_POSY                         = 2,
   SVGA3D_CUBEFACE_NEGY                         = 3,
   SVGA3D_CUBEFACE_POSZ                         = 4,
   SVGA3D_CUBEFACE_NEGZ                         = 5,
} SVGA3dCubeFace;

typedef enum {
   SVGA3D_SHADERTYPE_VS                         = 1,
   SVGA3D_SHADERTYPE_PS                         = 2,
   SVGA3D_SHADERTYPE_MAX
} SVGA3dShaderType;

typedef enum {
   SVGA3D_CONST_TYPE_FLOAT                      = 0,
   SVGA3D_CONST_TYPE_INT                        = 1,
   SVGA3D_CONST_TYPE_BOOL                       = 2,
} SVGA3dShaderConstType;

#define SVGA3D_MAX_SURFACE_FACES                6

typedef enum {
   SVGA3D_STRETCH_BLT_POINT                     = 0,
   SVGA3D_STRETCH_BLT_LINEAR                    = 1,
   SVGA3D_STRETCH_BLT_MAX
} SVGA3dStretchBltMode;

typedef enum {
   SVGA3D_QUERYTYPE_OCCLUSION                   = 0,
   SVGA3D_QUERYTYPE_MAX
} SVGA3dQueryType;

typedef enum {
   SVGA3D_QUERYSTATE_PENDING     = 0,      
   SVGA3D_QUERYSTATE_SUCCEEDED   = 1,      
   SVGA3D_QUERYSTATE_FAILED      = 2,      
   SVGA3D_QUERYSTATE_NEW         = 3,      
} SVGA3dQueryState;

typedef enum {
   SVGA3D_WRITE_HOST_VRAM        = 1,
   SVGA3D_READ_HOST_VRAM         = 2,
} SVGA3dTransferType;

#define SVGA3D_MAX_VERTEX_ARRAYS   32

#define SVGA3D_MAX_DRAW_PRIMITIVE_RANGES 32


#define SVGA_3D_CMD_LEGACY_BASE            1000
#define SVGA_3D_CMD_BASE                   1040

#define SVGA_3D_CMD_SURFACE_DEFINE         SVGA_3D_CMD_BASE + 0     
#define SVGA_3D_CMD_SURFACE_DESTROY        SVGA_3D_CMD_BASE + 1
#define SVGA_3D_CMD_SURFACE_COPY           SVGA_3D_CMD_BASE + 2
#define SVGA_3D_CMD_SURFACE_STRETCHBLT     SVGA_3D_CMD_BASE + 3
#define SVGA_3D_CMD_SURFACE_DMA            SVGA_3D_CMD_BASE + 4
#define SVGA_3D_CMD_CONTEXT_DEFINE         SVGA_3D_CMD_BASE + 5
#define SVGA_3D_CMD_CONTEXT_DESTROY        SVGA_3D_CMD_BASE + 6
#define SVGA_3D_CMD_SETTRANSFORM           SVGA_3D_CMD_BASE + 7
#define SVGA_3D_CMD_SETZRANGE              SVGA_3D_CMD_BASE + 8
#define SVGA_3D_CMD_SETRENDERSTATE         SVGA_3D_CMD_BASE + 9
#define SVGA_3D_CMD_SETRENDERTARGET        SVGA_3D_CMD_BASE + 10
#define SVGA_3D_CMD_SETTEXTURESTATE        SVGA_3D_CMD_BASE + 11
#define SVGA_3D_CMD_SETMATERIAL            SVGA_3D_CMD_BASE + 12
#define SVGA_3D_CMD_SETLIGHTDATA           SVGA_3D_CMD_BASE + 13
#define SVGA_3D_CMD_SETLIGHTENABLED        SVGA_3D_CMD_BASE + 14
#define SVGA_3D_CMD_SETVIEWPORT            SVGA_3D_CMD_BASE + 15
#define SVGA_3D_CMD_SETCLIPPLANE           SVGA_3D_CMD_BASE + 16
#define SVGA_3D_CMD_CLEAR                  SVGA_3D_CMD_BASE + 17
#define SVGA_3D_CMD_PRESENT                SVGA_3D_CMD_BASE + 18    
#define SVGA_3D_CMD_SHADER_DEFINE          SVGA_3D_CMD_BASE + 19
#define SVGA_3D_CMD_SHADER_DESTROY         SVGA_3D_CMD_BASE + 20
#define SVGA_3D_CMD_SET_SHADER             SVGA_3D_CMD_BASE + 21
#define SVGA_3D_CMD_SET_SHADER_CONST       SVGA_3D_CMD_BASE + 22
#define SVGA_3D_CMD_DRAW_PRIMITIVES        SVGA_3D_CMD_BASE + 23
#define SVGA_3D_CMD_SETSCISSORRECT         SVGA_3D_CMD_BASE + 24
#define SVGA_3D_CMD_BEGIN_QUERY            SVGA_3D_CMD_BASE + 25
#define SVGA_3D_CMD_END_QUERY              SVGA_3D_CMD_BASE + 26
#define SVGA_3D_CMD_WAIT_FOR_QUERY         SVGA_3D_CMD_BASE + 27
#define SVGA_3D_CMD_PRESENT_READBACK       SVGA_3D_CMD_BASE + 28    
#define SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN SVGA_3D_CMD_BASE + 29
#define SVGA_3D_CMD_SURFACE_DEFINE_V2      SVGA_3D_CMD_BASE + 30
#define SVGA_3D_CMD_GENERATE_MIPMAPS       SVGA_3D_CMD_BASE + 31
#define SVGA_3D_CMD_ACTIVATE_SURFACE       SVGA_3D_CMD_BASE + 40
#define SVGA_3D_CMD_DEACTIVATE_SURFACE     SVGA_3D_CMD_BASE + 41
#define SVGA_3D_CMD_MAX                    SVGA_3D_CMD_BASE + 42

#define SVGA_3D_CMD_FUTURE_MAX             2000


typedef struct {
   union {
      struct {
         uint16  function;       
         uint8   type;           
         uint8   base;           
      };
      uint32     uintValue;
   };
} SVGA3dFogMode;


typedef
struct SVGA3dSurfaceImageId {
   uint32               sid;
   uint32               face;
   uint32               mipmap;
} SVGA3dSurfaceImageId;

typedef
struct SVGA3dGuestImage {
   SVGAGuestPtr         ptr;

   uint32 pitch;
} SVGA3dGuestImage;



typedef
struct {
   uint32               id;
   uint32               size;
} SVGA3dCmdHeader;


typedef
struct {
   uint32               width;
   uint32               height;
   uint32               depth;
} SVGA3dSize;

typedef enum {
   SVGA3D_SURFACE_CUBEMAP              = (1 << 0),
   SVGA3D_SURFACE_HINT_STATIC          = (1 << 1),
   SVGA3D_SURFACE_HINT_DYNAMIC         = (1 << 2),
   SVGA3D_SURFACE_HINT_INDEXBUFFER     = (1 << 3),
   SVGA3D_SURFACE_HINT_VERTEXBUFFER    = (1 << 4),
   SVGA3D_SURFACE_HINT_TEXTURE         = (1 << 5),
   SVGA3D_SURFACE_HINT_RENDERTARGET    = (1 << 6),
   SVGA3D_SURFACE_HINT_DEPTHSTENCIL    = (1 << 7),
   SVGA3D_SURFACE_HINT_WRITEONLY       = (1 << 8),
   SVGA3D_SURFACE_MASKABLE_ANTIALIAS   = (1 << 9),
   SVGA3D_SURFACE_AUTOGENMIPMAPS       = (1 << 10),
} SVGA3dSurfaceFlags;

typedef
struct {
   uint32               numMipLevels;
} SVGA3dSurfaceFace;

typedef
struct {
   uint32                      sid;
   SVGA3dSurfaceFlags          surfaceFlags;
   SVGA3dSurfaceFormat         format;
   SVGA3dSurfaceFace           face[SVGA3D_MAX_SURFACE_FACES];
} SVGA3dCmdDefineSurface;       

typedef
struct {
   uint32                      sid;
   SVGA3dSurfaceFlags          surfaceFlags;
   SVGA3dSurfaceFormat         format;
   SVGA3dSurfaceFace           face[SVGA3D_MAX_SURFACE_FACES];
   uint32                      multisampleCount;
   SVGA3dTextureFilter         autogenFilter;
} SVGA3dCmdDefineSurface_v2;     

typedef
struct {
   uint32               sid;
} SVGA3dCmdDestroySurface;      

typedef
struct {
   uint32               cid;
} SVGA3dCmdDefineContext;       

typedef
struct {
   uint32               cid;
} SVGA3dCmdDestroyContext;      

typedef
struct {
   uint32               cid;
   SVGA3dClearFlag      clearFlag;
   uint32               color;
   float                depth;
   uint32               stencil;
   
} SVGA3dCmdClear;               

typedef
struct SVGA3dCopyRect {
   uint32               x;
   uint32               y;
   uint32               w;
   uint32               h;
   uint32               srcx;
   uint32               srcy;
} SVGA3dCopyRect;

typedef
struct SVGA3dCopyBox {
   uint32               x;
   uint32               y;
   uint32               z;
   uint32               w;
   uint32               h;
   uint32               d;
   uint32               srcx;
   uint32               srcy;
   uint32               srcz;
} SVGA3dCopyBox;

typedef
struct {
   uint32               x;
   uint32               y;
   uint32               w;
   uint32               h;
} SVGA3dRect;

typedef
struct {
   uint32               x;
   uint32               y;
   uint32               z;
   uint32               w;
   uint32               h;
   uint32               d;
} SVGA3dBox;

typedef
struct {
   uint32               x;
   uint32               y;
   uint32               z;
} SVGA3dPoint;

typedef
struct {
   SVGA3dLightType      type;
   SVGA3dBool           inWorldSpace;
   float                diffuse[4];
   float                specular[4];
   float                ambient[4];
   float                position[4];
   float                direction[4];
   float                range;
   float                falloff;
   float                attenuation0;
   float                attenuation1;
   float                attenuation2;
   float                theta;
   float                phi;
} SVGA3dLightData;

typedef
struct {
   uint32               sid;
   
} SVGA3dCmdPresent;             

typedef
struct {
   SVGA3dRenderStateName   state;
   union {
      uint32               uintValue;
      float                floatValue;
   };
} SVGA3dRenderState;

typedef
struct {
   uint32               cid;
   
} SVGA3dCmdSetRenderState;      

typedef
struct {
   uint32                 cid;
   SVGA3dRenderTargetType type;
   SVGA3dSurfaceImageId   target;
} SVGA3dCmdSetRenderTarget;     

typedef
struct {
   SVGA3dSurfaceImageId  src;
   SVGA3dSurfaceImageId  dest;
   
} SVGA3dCmdSurfaceCopy;               

typedef
struct {
   SVGA3dSurfaceImageId  src;
   SVGA3dSurfaceImageId  dest;
   SVGA3dBox             boxSrc;
   SVGA3dBox             boxDest;
   SVGA3dStretchBltMode  mode;
} SVGA3dCmdSurfaceStretchBlt;         

typedef
struct {
   uint32 discard : 1;

   uint32 unsynchronized : 1;

   uint32 reserved : 30;
} SVGA3dSurfaceDMAFlags;

typedef
struct {
   SVGA3dGuestImage      guest;
   SVGA3dSurfaceImageId  host;
   SVGA3dTransferType    transfer;
} SVGA3dCmdSurfaceDMA;                


typedef
struct {
   uint32 suffixSize;

   /*
    * The maximum offset is used to determine the maximum offset from the
    * guestPtr base address that will be accessed or written to during this
    * surfaceDMA.  If the suffix is supported, the host will respect this
    * boundary while performing surface DMAs.
    *
    * Defaults to MAX_UINT32
    */
   uint32 maximumOffset;

   SVGA3dSurfaceDMAFlags flags;
} SVGA3dCmdSurfaceDMASuffix;


typedef
struct {
   uint32               first;
   uint32               last;
} SVGA3dArrayRangeHint;

typedef
struct {
   uint32               surfaceId;
   uint32               offset;
   uint32               stride;
} SVGA3dArray;

typedef
struct {
   SVGA3dDeclType       type;
   SVGA3dDeclMethod     method;
   SVGA3dDeclUsage      usage;
   uint32               usageIndex;
} SVGA3dVertexArrayIdentity;

typedef
struct {
   SVGA3dVertexArrayIdentity  identity;
   SVGA3dArray                array;
   SVGA3dArrayRangeHint       rangeHint;
} SVGA3dVertexDecl;

typedef
struct {
   SVGA3dPrimitiveType  primType;
   uint32               primitiveCount;

   SVGA3dArray          indexArray;
   uint32               indexWidth;

   int32                indexBias;
} SVGA3dPrimitiveRange;

typedef
struct {
   uint32               cid;
   uint32               numVertexDecls;
   uint32               numRanges;

} SVGA3dCmdDrawPrimitives;      

typedef
struct {
   uint32                   stage;
   SVGA3dTextureStateName   name;
   union {
      uint32                value;
      float                 floatValue;
   };
} SVGA3dTextureState;

typedef
struct {
   uint32               cid;
   
} SVGA3dCmdSetTextureState;      

typedef
struct {
   uint32                   cid;
   SVGA3dTransformType      type;
   float                    matrix[16];
} SVGA3dCmdSetTransform;          

typedef
struct {
   float                min;
   float                max;
} SVGA3dZRange;

typedef
struct {
   uint32               cid;
   SVGA3dZRange         zRange;
} SVGA3dCmdSetZRange;             

typedef
struct {
   float                diffuse[4];
   float                ambient[4];
   float                specular[4];
   float                emissive[4];
   float                shininess;
} SVGA3dMaterial;

typedef
struct {
   uint32               cid;
   SVGA3dFace           face;
   SVGA3dMaterial       material;
} SVGA3dCmdSetMaterial;           

typedef
struct {
   uint32               cid;
   uint32               index;
   SVGA3dLightData      data;
} SVGA3dCmdSetLightData;           

typedef
struct {
   uint32               cid;
   uint32               index;
   uint32               enabled;
} SVGA3dCmdSetLightEnabled;      

typedef
struct {
   uint32               cid;
   SVGA3dRect           rect;
} SVGA3dCmdSetViewport;           

typedef
struct {
   uint32               cid;
   SVGA3dRect           rect;
} SVGA3dCmdSetScissorRect;         

typedef
struct {
   uint32               cid;
   uint32               index;
   float                plane[4];
} SVGA3dCmdSetClipPlane;           

typedef
struct {
   uint32               cid;
   uint32               shid;
   SVGA3dShaderType     type;
   
} SVGA3dCmdDefineShader;           

typedef
struct {
   uint32               cid;
   uint32               shid;
   SVGA3dShaderType     type;
} SVGA3dCmdDestroyShader;         

typedef
struct {
   uint32                  cid;
   uint32                  reg;     
   SVGA3dShaderType        type;
   SVGA3dShaderConstType   ctype;
   uint32                  values[4];
} SVGA3dCmdSetShaderConst;        

typedef
struct {
   uint32               cid;
   SVGA3dShaderType     type;
   uint32               shid;
} SVGA3dCmdSetShader;             

typedef
struct {
   uint32               cid;
   SVGA3dQueryType      type;
} SVGA3dCmdBeginQuery;           

typedef
struct {
   uint32               cid;
   SVGA3dQueryType      type;
   SVGAGuestPtr         guestResult;  
} SVGA3dCmdEndQuery;                  

typedef
struct {
   uint32               cid;          
   SVGA3dQueryType      type;
   SVGAGuestPtr         guestResult;
} SVGA3dCmdWaitForQuery;              

typedef
struct {
   uint32               totalSize;    
   SVGA3dQueryState     state;        
   union {                            
      uint32            result32;
   };
} SVGA3dQueryResult;


typedef
struct {
   SVGA3dSurfaceImageId srcImage;
   SVGASignedRect       srcRect;
   uint32               destScreenId; 
   SVGASignedRect       destRect;     
   
} SVGA3dCmdBlitSurfaceToScreen;         

typedef
struct {
   uint32               sid;
   SVGA3dTextureFilter  filter;
} SVGA3dCmdGenerateMipmaps;             



typedef enum {
   SVGA3D_DEVCAP_3D                                = 0,
   SVGA3D_DEVCAP_MAX_LIGHTS                        = 1,
   SVGA3D_DEVCAP_MAX_TEXTURES                      = 2,  
   SVGA3D_DEVCAP_MAX_CLIP_PLANES                   = 3,
   SVGA3D_DEVCAP_VERTEX_SHADER_VERSION             = 4,
   SVGA3D_DEVCAP_VERTEX_SHADER                     = 5,
   SVGA3D_DEVCAP_FRAGMENT_SHADER_VERSION           = 6,
   SVGA3D_DEVCAP_FRAGMENT_SHADER                   = 7,
   SVGA3D_DEVCAP_MAX_RENDER_TARGETS                = 8,
   SVGA3D_DEVCAP_S23E8_TEXTURES                    = 9,
   SVGA3D_DEVCAP_S10E5_TEXTURES                    = 10,
   SVGA3D_DEVCAP_MAX_FIXED_VERTEXBLEND             = 11,
   SVGA3D_DEVCAP_D16_BUFFER_FORMAT                 = 12, 
   SVGA3D_DEVCAP_D24S8_BUFFER_FORMAT               = 13, 
   SVGA3D_DEVCAP_D24X8_BUFFER_FORMAT               = 14, 
   SVGA3D_DEVCAP_QUERY_TYPES                       = 15,
   SVGA3D_DEVCAP_TEXTURE_GRADIENT_SAMPLING         = 16,
   SVGA3D_DEVCAP_MAX_POINT_SIZE                    = 17,
   SVGA3D_DEVCAP_MAX_SHADER_TEXTURES               = 18,
   SVGA3D_DEVCAP_MAX_TEXTURE_WIDTH                 = 19,
   SVGA3D_DEVCAP_MAX_TEXTURE_HEIGHT                = 20,
   SVGA3D_DEVCAP_MAX_VOLUME_EXTENT                 = 21,
   SVGA3D_DEVCAP_MAX_TEXTURE_REPEAT                = 22,
   SVGA3D_DEVCAP_MAX_TEXTURE_ASPECT_RATIO          = 23,
   SVGA3D_DEVCAP_MAX_TEXTURE_ANISOTROPY            = 24,
   SVGA3D_DEVCAP_MAX_PRIMITIVE_COUNT               = 25,
   SVGA3D_DEVCAP_MAX_VERTEX_INDEX                  = 26,
   SVGA3D_DEVCAP_MAX_VERTEX_SHADER_INSTRUCTIONS    = 27,
   SVGA3D_DEVCAP_MAX_FRAGMENT_SHADER_INSTRUCTIONS  = 28,
   SVGA3D_DEVCAP_MAX_VERTEX_SHADER_TEMPS           = 29,
   SVGA3D_DEVCAP_MAX_FRAGMENT_SHADER_TEMPS         = 30,
   SVGA3D_DEVCAP_TEXTURE_OPS                       = 31,
   SVGA3D_DEVCAP_SURFACEFMT_X8R8G8B8               = 32,
   SVGA3D_DEVCAP_SURFACEFMT_A8R8G8B8               = 33,
   SVGA3D_DEVCAP_SURFACEFMT_A2R10G10B10            = 34,
   SVGA3D_DEVCAP_SURFACEFMT_X1R5G5B5               = 35,
   SVGA3D_DEVCAP_SURFACEFMT_A1R5G5B5               = 36,
   SVGA3D_DEVCAP_SURFACEFMT_A4R4G4B4               = 37,
   SVGA3D_DEVCAP_SURFACEFMT_R5G6B5                 = 38,
   SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE16            = 39,
   SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8_ALPHA8      = 40,
   SVGA3D_DEVCAP_SURFACEFMT_ALPHA8                 = 41,
   SVGA3D_DEVCAP_SURFACEFMT_LUMINANCE8             = 42,
   SVGA3D_DEVCAP_SURFACEFMT_Z_D16                  = 43,
   SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8                = 44,
   SVGA3D_DEVCAP_SURFACEFMT_Z_D24X8                = 45,
   SVGA3D_DEVCAP_SURFACEFMT_DXT1                   = 46,
   SVGA3D_DEVCAP_SURFACEFMT_DXT2                   = 47,
   SVGA3D_DEVCAP_SURFACEFMT_DXT3                   = 48,
   SVGA3D_DEVCAP_SURFACEFMT_DXT4                   = 49,
   SVGA3D_DEVCAP_SURFACEFMT_DXT5                   = 50,
   SVGA3D_DEVCAP_SURFACEFMT_BUMPX8L8V8U8           = 51,
   SVGA3D_DEVCAP_SURFACEFMT_A2W10V10U10            = 52,
   SVGA3D_DEVCAP_SURFACEFMT_BUMPU8V8               = 53,
   SVGA3D_DEVCAP_SURFACEFMT_Q8W8V8U8               = 54,
   SVGA3D_DEVCAP_SURFACEFMT_CxV8U8                 = 55,
   SVGA3D_DEVCAP_SURFACEFMT_R_S10E5                = 56,
   SVGA3D_DEVCAP_SURFACEFMT_R_S23E8                = 57,
   SVGA3D_DEVCAP_SURFACEFMT_RG_S10E5               = 58,
   SVGA3D_DEVCAP_SURFACEFMT_RG_S23E8               = 59,
   SVGA3D_DEVCAP_SURFACEFMT_ARGB_S10E5             = 60,
   SVGA3D_DEVCAP_SURFACEFMT_ARGB_S23E8             = 61,
   SVGA3D_DEVCAP_MAX_VERTEX_SHADER_TEXTURES        = 63,

   SVGA3D_DEVCAP_MAX_SIMULTANEOUS_RENDER_TARGETS   = 64,

   SVGA3D_DEVCAP_SURFACEFMT_V16U16                 = 65,
   SVGA3D_DEVCAP_SURFACEFMT_G16R16                 = 66,
   SVGA3D_DEVCAP_SURFACEFMT_A16B16G16R16           = 67,
   SVGA3D_DEVCAP_SURFACEFMT_UYVY                   = 68,
   SVGA3D_DEVCAP_SURFACEFMT_YUY2                   = 69,
   SVGA3D_DEVCAP_MULTISAMPLE_NONMASKABLESAMPLES    = 70,
   SVGA3D_DEVCAP_MULTISAMPLE_MASKABLESAMPLES       = 71,
   SVGA3D_DEVCAP_ALPHATOCOVERAGE                   = 72,
   SVGA3D_DEVCAP_SUPERSAMPLE                       = 73,
   SVGA3D_DEVCAP_AUTOGENMIPMAPS                    = 74,
   SVGA3D_DEVCAP_SURFACEFMT_NV12                   = 75,
   SVGA3D_DEVCAP_SURFACEFMT_AYUV                   = 76,

   SVGA3D_DEVCAP_MAX_CONTEXT_IDS                   = 77,

   SVGA3D_DEVCAP_MAX_SURFACE_IDS                   = 78,

   SVGA3D_DEVCAP_SURFACEFMT_Z_DF16                 = 79,
   SVGA3D_DEVCAP_SURFACEFMT_Z_DF24                 = 80,
   SVGA3D_DEVCAP_SURFACEFMT_Z_D24S8_INT            = 81,

   SVGA3D_DEVCAP_SURFACEFMT_BC4_UNORM              = 82,
   SVGA3D_DEVCAP_SURFACEFMT_BC5_UNORM              = 83,

   SVGA3D_DEVCAP_MAX                                  
} SVGA3dDevCapIndex;

typedef union {
   Bool   b;
   uint32 u;
   int32  i;
   float  f;
} SVGA3dDevCapResult;

#endif 
