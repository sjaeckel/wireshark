/* Do not modify this file. */
/* It was automatically generated by ../../tools/process-x11-xcb.pl
   using xcbproto version 1.6-26-gfb2af7c */
/* $Id$ */

/*
 * Copyright 2008, 2009 Open Text Corporation <pharris[AT]opentext.com>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald[AT]wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

static const value_string x11_enum_PictType[] = {
	{   0, "Indexed" },
	{   1, "Direct" },
	{   0, NULL },
};

static const value_string x11_enum_Redirect[] = {
	{   0, "Automatic" },
	{   1, "Manual" },
	{   0, NULL },
};

static const value_string x11_enum_ReportLevel[] = {
	{   0, "RawRectangles" },
	{   1, "DeltaRectangles" },
	{   2, "BoundingBox" },
	{   3, "NonEmpty" },
	{   0, NULL },
};

static const value_string x11_enum_Region[] = {
	{   0, "None" },
	{   0, NULL },
};

static const value_string x11_enum_DPMSMode[] = {
	{   0, "On" },
	{   1, "Standby" },
	{   2, "Suspend" },
	{   3, "Off" },
	{   0, NULL },
};

static const value_string x11_enum_Attachment[] = {
	{   0, "BufferFrontLeft" },
	{   1, "BufferBackLeft" },
	{   2, "BufferFrontRight" },
	{   3, "BufferBackRight" },
	{   4, "BufferDepth" },
	{   5, "BufferStencil" },
	{   6, "BufferAccum" },
	{   7, "BufferFakeFrontLeft" },
	{   8, "BufferFakeFrontRight" },
	{   9, "BufferDepthStencil" },
	{   0, NULL },
};

static const value_string x11_enum_DriverType[] = {
	{   0, "DRI" },
	{   0, NULL },
};

static const value_string x11_enum_SetConfig[] = {
	{   0, "Success" },
	{   1, "InvalidConfigTime" },
	{   2, "InvalidTime" },
	{   3, "Failed" },
	{   0, NULL },
};

static const value_string x11_enum_SubPixel[] = {
	{   0, "Unknown" },
	{   1, "HorizontalRGB" },
	{   2, "HorizontalBGR" },
	{   3, "VerticalRGB" },
	{   4, "VerticalBGR" },
	{   5, "None" },
	{   0, NULL },
};

static const value_string x11_enum_Connection[] = {
	{   0, "Connected" },
	{   1, "Disconnected" },
	{   2, "Unknown" },
	{   0, NULL },
};

static const value_string x11_enum_PropMode[] = {
	{   0, "Replace" },
	{   1, "Prepend" },
	{   2, "Append" },
	{   0, NULL },
};

static const value_string x11_enum_GetPropertyType[] = {
	{   0, "Any" },
	{   0, NULL },
};

static const value_string x11_enum_Atom[] = {
	{   0, "Any" },
	{   1, "PRIMARY" },
	{   2, "SECONDARY" },
	{   3, "ARC" },
	{   4, "ATOM" },
	{   5, "BITMAP" },
	{   6, "CARDINAL" },
	{   7, "COLORMAP" },
	{   8, "CURSOR" },
	{   9, "CUT_BUFFER0" },
	{  10, "CUT_BUFFER1" },
	{  11, "CUT_BUFFER2" },
	{  12, "CUT_BUFFER3" },
	{  13, "CUT_BUFFER4" },
	{  14, "CUT_BUFFER5" },
	{  15, "CUT_BUFFER6" },
	{  16, "CUT_BUFFER7" },
	{  17, "DRAWABLE" },
	{  18, "FONT" },
	{  19, "INTEGER" },
	{  20, "PIXMAP" },
	{  21, "POINT" },
	{  22, "RECTANGLE" },
	{  23, "RESOURCE_MANAGER" },
	{  24, "RGB_COLOR_MAP" },
	{  25, "RGB_BEST_MAP" },
	{  26, "RGB_BLUE_MAP" },
	{  27, "RGB_DEFAULT_MAP" },
	{  28, "RGB_GRAY_MAP" },
	{  29, "RGB_GREEN_MAP" },
	{  30, "RGB_RED_MAP" },
	{  31, "STRING" },
	{  32, "VISUALID" },
	{  33, "WINDOW" },
	{  34, "WM_COMMAND" },
	{  35, "WM_HINTS" },
	{  36, "WM_CLIENT_MACHINE" },
	{  37, "WM_ICON_NAME" },
	{  38, "WM_ICON_SIZE" },
	{  39, "WM_NAME" },
	{  40, "WM_NORMAL_HINTS" },
	{  41, "WM_SIZE_HINTS" },
	{  42, "WM_ZOOM_HINTS" },
	{  43, "MIN_SPACE" },
	{  44, "NORM_SPACE" },
	{  45, "MAX_SPACE" },
	{  46, "END_SPACE" },
	{  47, "SUPERSCRIPT_X" },
	{  48, "SUPERSCRIPT_Y" },
	{  49, "SUBSCRIPT_X" },
	{  50, "SUBSCRIPT_Y" },
	{  51, "UNDERLINE_POSITION" },
	{  52, "UNDERLINE_THICKNESS" },
	{  53, "STRIKEOUT_ASCENT" },
	{  54, "STRIKEOUT_DESCENT" },
	{  55, "ITALIC_ANGLE" },
	{  56, "X_HEIGHT" },
	{  57, "QUAD_WIDTH" },
	{  58, "WEIGHT" },
	{  59, "POINT_SIZE" },
	{  60, "RESOLUTION" },
	{  61, "COPYRIGHT" },
	{  62, "NOTICE" },
	{  63, "FONT_NAME" },
	{  64, "FAMILY_NAME" },
	{  65, "FULL_NAME" },
	{  66, "CAP_HEIGHT" },
	{  67, "WM_CLASS" },
	{  68, "WM_TRANSIENT_FOR" },
	{   0, NULL },
};

static const value_string x11_enum_Property[] = {
	{   0, "NewValue" },
	{   1, "Delete" },
	{   0, NULL },
};

static const value_string x11_enum_Notify[] = {
	{   0, "CrtcChange" },
	{   1, "OutputChange" },
	{   2, "OutputProperty" },
	{   0, NULL },
};

static const value_string x11_enum_PictOp[] = {
	{   0, "Clear" },
	{   1, "Src" },
	{   2, "Dst" },
	{   3, "Over" },
	{   4, "OverReverse" },
	{   5, "In" },
	{   6, "InReverse" },
	{   7, "Out" },
	{   8, "OutReverse" },
	{   9, "Atop" },
	{  10, "AtopReverse" },
	{  11, "Xor" },
	{  12, "Add" },
	{  13, "Saturate" },
	{  16, "DisjointClear" },
	{  17, "DisjointSrc" },
	{  18, "DisjointDst" },
	{  19, "DisjointOver" },
	{  20, "DisjointOverReverse" },
	{  21, "DisjointIn" },
	{  22, "DisjointInReverse" },
	{  23, "DisjointOut" },
	{  24, "DisjointOutReverse" },
	{  25, "DisjointAtop" },
	{  26, "DisjointAtopReverse" },
	{  27, "DisjointXor" },
	{  32, "ConjointClear" },
	{  33, "ConjointSrc" },
	{  34, "ConjointDst" },
	{  35, "ConjointOver" },
	{  36, "ConjointOverReverse" },
	{  37, "ConjointIn" },
	{  38, "ConjointInReverse" },
	{  39, "ConjointOut" },
	{  40, "ConjointOutReverse" },
	{  41, "ConjointAtop" },
	{  42, "ConjointAtopReverse" },
	{  43, "ConjointXor" },
	{   0, NULL },
};

static const value_string x11_enum_Picture[] = {
	{   0, "None" },
	{   0, NULL },
};

static const value_string x11_enum_SK[] = {
	{   0, "Bounding" },
	{   1, "Clip" },
	{   2, "Input" },
	{   0, NULL },
};

static const value_string x11_enum_SO[] = {
	{   0, "Set" },
	{   1, "Union" },
	{   2, "Intersect" },
	{   3, "Subtract" },
	{   4, "Invert" },
	{   0, NULL },
};

static const value_string x11_enum_ClipOrdering[] = {
	{   0, "Unsorted" },
	{   1, "YSorted" },
	{   2, "YXSorted" },
	{   3, "YXBanded" },
	{   0, NULL },
};

static const value_string x11_enum_Pixmap[] = {
	{   0, "None" },
	{   0, NULL },
};

static const value_string x11_enum_VALUETYPE[] = {
	{   0, "Absolute" },
	{   1, "Relative" },
	{   0, NULL },
};

static const value_string x11_enum_TESTTYPE[] = {
	{   0, "PositiveTransition" },
	{   1, "NegativeTransition" },
	{   2, "PositiveComparison" },
	{   3, "NegativeComparison" },
	{   0, NULL },
};

static const value_string x11_enum_ALARMSTATE[] = {
	{   0, "Active" },
	{   1, "Inactive" },
	{   2, "Destroyed" },
	{   0, NULL },
};

static const value_string x11_enum_SaveSetMode[] = {
	{   0, "Insert" },
	{   1, "Delete" },
	{   0, NULL },
};

static const value_string x11_enum_SaveSetTarget[] = {
	{   0, "Nearest" },
	{   1, "Root" },
	{   0, NULL },
};

static const value_string x11_enum_SaveSetMapping[] = {
	{   0, "Map" },
	{   1, "Unmap" },
	{   0, NULL },
};

static const value_string x11_enum_SelectionEvent[] = {
	{   0, "SetSelectionOwner" },
	{   1, "SelectionWindowDestroy" },
	{   2, "SelectionClientClose" },
	{   0, NULL },
};

static const value_string x11_enum_CursorNotify[] = {
	{   0, "DisplayCursor" },
	{   0, NULL },
};

static const value_string x11_enum_DeviceUse[] = {
	{   0, "IsXPointer" },
	{   1, "IsXKeyboard" },
	{   2, "IsXExtensionDevice" },
	{   3, "IsXExtensionKeyboard" },
	{   4, "IsXExtensionPointer" },
	{   0, NULL },
};

static const value_string x11_enum_InputClass[] = {
	{   0, "Key" },
	{   1, "Button" },
	{   2, "Valuator" },
	{   3, "Feedback" },
	{   4, "Proximity" },
	{   5, "Focus" },
	{   6, "Other" },
	{   0, NULL },
};

static const value_string x11_enum_ValuatorMode[] = {
	{   0, "Relative" },
	{   1, "Absolute" },
	{   0, NULL },
};

static const value_string x11_enum_GrabStatus[] = {
	{   0, "Success" },
	{   1, "AlreadyGrabbed" },
	{   2, "InvalidTime" },
	{   3, "NotViewable" },
	{   4, "Frozen" },
	{   0, NULL },
};

static const value_string x11_enum_PropagateMode[] = {
	{   0, "AddToList" },
	{   1, "DeleteFromList" },
	{   0, NULL },
};

static const value_string x11_enum_Time[] = {
	{   0, "CurrentTime" },
	{   0, NULL },
};

static const value_string x11_enum_GrabMode[] = {
	{   0, "Sync" },
	{   1, "Async" },
	{   0, NULL },
};

static const value_string x11_enum_Grab[] = {
	{   0, "Any" },
	{   0, NULL },
};

static const value_string x11_enum_DeviceInputMode[] = {
	{   0, "AsyncThisDevice" },
	{   1, "SyncThisDevice" },
	{   2, "ReplayThisDevice" },
	{   3, "AsyncOtherDevices" },
	{   4, "AsyncAll" },
	{   5, "SyncAll" },
	{   0, NULL },
};

static const value_string x11_enum_InputFocus[] = {
	{   0, "None" },
	{   1, "PointerRoot" },
	{   2, "Parent" },
	{   3, "FollowKeyboard" },
	{   0, NULL },
};

static const value_string x11_enum_MappingStatus[] = {
	{   0, "Success" },
	{   1, "Busy" },
	{   2, "Failure" },
	{   0, NULL },
};

static const value_string x11_enum_Window[] = {
	{   0, "None" },
	{   0, NULL },
};

static const value_string x11_enum_NotifyDetail[] = {
	{   0, "Ancestor" },
	{   1, "Virtual" },
	{   2, "Inferior" },
	{   3, "Nonlinear" },
	{   4, "NonlinearVirtual" },
	{   5, "Pointer" },
	{   6, "PointerRoot" },
	{   7, "None" },
	{   0, NULL },
};

static const value_string x11_enum_NotifyMode[] = {
	{   0, "Normal" },
	{   1, "Grab" },
	{   2, "Ungrab" },
	{   3, "WhileGrabbed" },
	{   0, NULL },
};

static const value_string x11_enum_AXFBOpt[] = {
	{   0, NULL },
};

static const value_string x11_enum_AXSKOpt[] = {
	{   0, NULL },
};

static const value_string x11_enum_IMFlag[] = {
	{   0, NULL },
};

static const value_string x11_enum_IMGroupsWhich[] = {
	{   0, NULL },
};

static const value_string x11_enum_SetOfGroup[] = {
	{   0, NULL },
};

static const value_string x11_enum_IMModsWhich[] = {
	{   0, NULL },
};

static const value_string x11_enum_BoolCtrl[] = {
	{   0, NULL },
};

static const value_string x11_enum_DoodadType[] = {
	{   1, "Outline" },
	{   2, "Solid" },
	{   3, "Text" },
	{   4, "Indicator" },
	{   5, "Logo" },
	{   0, NULL },
};

static const value_string x11_enum_LedClass[] = {
	{ 768, "DfltXIClass" },
	{ 1280, "AllXIClasses" },
	{   0, NULL },
};

static const value_string x11_enum_ID[] = {
	{ 256, "UseCoreKbd" },
	{ 512, "UseCorePtr" },
	{ 768, "DfltXIClass" },
	{ 1024, "DfltXIId" },
	{ 1280, "AllXIClass" },
	{ 1536, "AllXIId" },
	{ 65280, "XINone" },
	{   0, NULL },
};

static const value_string x11_enum_SAType[] = {
	{   0, "NoAction" },
	{   1, "SetMods" },
	{   2, "LatchMods" },
	{   3, "LockMods" },
	{   4, "SetGroup" },
	{   5, "LatchGroup" },
	{   6, "LockGroup" },
	{   7, "MovePtr" },
	{   8, "PtrBtn" },
	{   9, "LockPtrBtn" },
	{  10, "SetPtrDflt" },
	{  11, "ISOLock" },
	{  12, "Terminate" },
	{  13, "SwitchScreen" },
	{  14, "SetControls" },
	{  15, "LockControls" },
	{  16, "ActionMessage" },
	{  17, "RedirectKey" },
	{  18, "DeviceBtn" },
	{  19, "LockDeviceBtn" },
	{  20, "DeviceValuator" },
	{   0, NULL },
};

static const value_string x11_enum_SAValWhat[] = {
	{   0, "IgnoreVal" },
	{   1, "SetValMin" },
	{   2, "SetValCenter" },
	{   3, "SetValMax" },
	{   4, "SetValRelative" },
	{   5, "SetValAbsolute" },
	{   0, NULL },
};

static const value_string x11_enum_EventType[] = {
	{   0, NULL },
};

static const value_string x11_enum_MapPart[] = {
	{   0, NULL },
};

static const value_string x11_enum_Group[] = {
	{   0, "1" },
	{   1, "2" },
	{   2, "3" },
	{   3, "4" },
	{   0, NULL },
};

static const value_string x11_enum_BellClassResult[] = {
	{   0, "KbdFeedbackClass" },
	{   5, "BellFeedbackClass" },
	{   0, NULL },
};

static const value_string x11_enum_LedClassResult[] = {
	{   0, "KbdFeedbackClass" },
	{   4, "LedFeedbackClass" },
	{   0, NULL },
};

static const value_string x11_enum_ImageFormatInfoType[] = {
	{   0, "RGB" },
	{   1, "YUV" },
	{   0, NULL },
};

static const value_string x11_enum_ImageOrder[] = {
	{   0, "LSBFirst" },
	{   1, "MSBFirst" },
	{   0, NULL },
};

static const value_string x11_enum_ImageFormatInfoFormat[] = {
	{   0, "Packed" },
	{   1, "Planar" },
	{   0, NULL },
};

static const value_string x11_enum_ScanlineOrder[] = {
	{   0, "TopToBottom" },
	{   1, "BottomToTop" },
	{   0, NULL },
};

static const value_string x11_enum_VideoNotifyReason[] = {
	{   0, "Started" },
	{   1, "Stopped" },
	{   2, "Busy" },
	{   3, "Preempted" },
	{   4, "HardError" },
	{   0, NULL },
};

static const value_string x11_enum_GrabPortStatus[] = {
	{   0, "Success" },
	{   1, "BadExtension" },
	{   2, "AlreadyGrabbed" },
	{   3, "InvalidTime" },
	{   4, "BadReply" },
	{   5, "BadAlloc" },
	{   0, NULL },
};

