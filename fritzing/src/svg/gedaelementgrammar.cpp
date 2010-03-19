// This file was generated by qlalr - DO NOT EDIT!
#include "gedaelementgrammar_p.h"

const char *const GedaElementGrammar::spell [] = {
  "end of file", 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 
#ifndef QLALR_NO_GEDAELEMENTGRAMMAR_DEBUG_INFO
"geda_element", "element_command", "element_command_sequence", "sub_element_sequence", "sub_element_sequence_paren", "sub_element_sequence_bracket", 
  "sub_element_groups", "element_command_sequence_paren", "element_command_sequence_bracket", "element_arguments", "SFlags", "description", "pcb_name", "value", "mark_x", "mark_y", 
  "text_x", "text_y", "text_direction", "text_scale", "sub_element_group", "pin_element", "pad_element", "element_arc_element", "element_line_element", "mark_element", 
  "mark_command", "mark_sequence", "mark_paren_sequence", "mark_bracket_sequence", "mark_arguments", "x", "y", "pin_command", "pin_sequence", "pin_paren_sequence", 
  "pin_bracket_sequence", "pin_arguments", "pin_arguments_1", "pin_arguments_2", "pin_arguments_3", "pin_arguments_4", "Thickness", "Clearance", "Mask", "DrillHole", 
  "Name", "pin_number", "NFlags", "pad_command", "pad_sequence", "pad_paren_sequence", "pad_bracket_sequence", "pad_arguments", "pad_arguments_1", "pad_arguments_2", 
  "pad_arguments_3", "x1", "y1", "x2", "y2", "pad_number", "element_line_command", "element_line_sequence", "element_line_paren_sequence", "element_line_bracket_sequence", 
  "element_line_arguments", "element_arc_command", "element_arc_sequence", "element_arc_paren_sequence", "element_arc_bracket_sequence", "element_arc_arguments", "Width", "Height", "StartAngle", "Delta", 
  "string_value", "number_value", "hex_number_value", "$accept"
#endif // QLALR_NO_GEDAELEMENTGRAMMAR_DEBUG_INFO
};

const int GedaElementGrammar::lhs [] = {
  14, 17, 17, 18, 19, 16, 16, 21, 22, 23, 
  20, 20, 34, 34, 34, 34, 34, 39, 41, 41, 
  42, 43, 44, 35, 48, 48, 49, 50, 51, 51, 
  51, 51, 52, 53, 54, 55, 36, 64, 64, 65, 
  66, 67, 67, 67, 68, 69, 70, 38, 77, 77, 
  78, 79, 80, 37, 82, 82, 83, 84, 85, 75, 
  45, 71, 73, 46, 72, 74, 56, 57, 58, 59, 
  60, 61, 86, 87, 88, 89, 24, 24, 62, 25, 
  26, 27, 28, 29, 30, 31, 32, 33, 91, 92, 
  90, 15, 47, 63, 40, 76, 81, 93};

const int GedaElementGrammar:: rhs[] = {
  3, 1, 1, 3, 3, 1, 1, 3, 3, 11, 
  1, 2, 1, 1, 1, 1, 1, 2, 1, 1, 
  3, 3, 2, 2, 1, 1, 3, 3, 1, 1, 
  1, 1, 9, 7, 6, 5, 2, 1, 1, 3, 
  3, 1, 1, 1, 10, 8, 7, 2, 1, 1, 
  3, 3, 5, 2, 1, 1, 3, 3, 7, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 2};


#ifndef QLALR_NO_GEDAELEMENTGRAMMAR_DEBUG_INFO
const int GedaElementGrammar::rule_info [] = {
    14, 15, 16, 17
  , 17, 18
  , 17, 19
  , 18, 7, 20, 8
  , 19, 9, 20, 10
  , 16, 21
  , 16, 22
  , 21, 7, 23, 8
  , 22, 9, 23, 10
  , 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 24
  , 20, 34
  , 20, 34, 20
  , 34, 35
  , 34, 36
  , 34, 37
  , 34, 38
  , 34, 39
  , 39, 40, 41
  , 41, 42
  , 41, 43
  , 42, 7, 44, 8
  , 43, 9, 44, 10
  , 44, 45, 46
  , 35, 47, 48
  , 48, 49
  , 48, 50
  , 49, 7, 51, 8
  , 50, 9, 51, 10
  , 51, 52
  , 51, 53
  , 51, 54
  , 51, 55
  , 52, 45, 46, 56, 57, 58, 59, 60, 61, 24
  , 53, 45, 46, 56, 59, 60, 61, 62
  , 54, 45, 46, 56, 59, 60, 62
  , 55, 45, 46, 56, 60, 62
  , 36, 63, 64
  , 64, 65
  , 64, 66
  , 65, 7, 67, 8
  , 66, 9, 67, 10
  , 67, 68
  , 67, 69
  , 67, 70
  , 68, 71, 72, 73, 74, 56, 57, 58, 60, 75, 24
  , 69, 71, 72, 73, 74, 56, 60, 75, 62
  , 70, 71, 72, 73, 74, 56, 60, 62
  , 38, 76, 77
  , 77, 78
  , 77, 79
  , 78, 7, 80, 8
  , 79, 9, 80, 10
  , 80, 71, 72, 73, 74, 56
  , 37, 81, 82
  , 82, 83
  , 82, 84
  , 83, 7, 85, 8
  , 84, 9, 85, 10
  , 85, 45, 46, 86, 87, 88, 89, 56
  , 75, 90
  , 45, 91
  , 71, 91
  , 73, 91
  , 46, 91
  , 72, 91
  , 74, 91
  , 56, 91
  , 57, 91
  , 58, 91
  , 59, 91
  , 60, 90
  , 61, 90
  , 86, 91
  , 87, 91
  , 88, 91
  , 89, 91
  , 24, 90
  , 24, 92
  , 62, 92
  , 25, 90
  , 26, 90
  , 27, 90
  , 28, 91
  , 29, 91
  , 30, 91
  , 31, 91
  , 32, 91
  , 33, 91
  , 91, 11
  , 92, 13
  , 90, 12
  , 15, 1
  , 47, 3
  , 63, 2
  , 40, 4
  , 76, 5
  , 81, 6
  , 93, 14, 0};

const int GedaElementGrammar::rule_index [] = {
  0, 4, 6, 8, 12, 16, 18, 20, 24, 28, 
  40, 42, 45, 47, 49, 51, 53, 55, 58, 60, 
  62, 66, 70, 73, 76, 78, 80, 84, 88, 90, 
  92, 94, 96, 106, 114, 121, 127, 130, 132, 134, 
  138, 142, 144, 146, 148, 159, 168, 176, 179, 181, 
  183, 187, 191, 197, 200, 202, 204, 208, 212, 220, 
  222, 224, 226, 228, 230, 232, 234, 236, 238, 240, 
  242, 244, 246, 248, 250, 252, 254, 256, 258, 260, 
  262, 264, 266, 268, 270, 272, 274, 276, 278, 280, 
  282, 284, 286, 288, 290, 292, 294, 296};
#endif // QLALR_NO_GEDAELEMENTGRAMMAR_DEBUG_INFO

const int GedaElementGrammar::action_default [] = {
  0, 92, 0, 0, 0, 0, 0, 7, 6, 90, 
  0, 91, 0, 78, 77, 0, 80, 0, 81, 82, 
  0, 89, 0, 83, 0, 84, 85, 0, 86, 0, 
  87, 0, 88, 0, 10, 9, 0, 8, 0, 0, 
  1, 3, 2, 97, 96, 95, 94, 93, 0, 15, 
  0, 16, 0, 17, 0, 14, 0, 13, 11, 0, 
  0, 0, 56, 55, 54, 0, 61, 0, 58, 64, 
  0, 0, 73, 0, 74, 0, 75, 0, 76, 59, 
  67, 0, 57, 0, 0, 50, 49, 48, 0, 62, 
  0, 52, 65, 0, 63, 0, 66, 0, 53, 0, 
  51, 0, 0, 20, 19, 18, 0, 0, 22, 23, 
  0, 21, 0, 0, 39, 38, 37, 0, 42, 43, 
  44, 0, 41, 0, 0, 0, 0, 0, 0, 68, 
  71, 0, 69, 0, 0, 60, 45, 47, 79, 0, 
  46, 0, 40, 0, 0, 26, 25, 24, 0, 29, 
  30, 31, 32, 0, 28, 0, 0, 0, 0, 0, 
  70, 0, 0, 70, 0, 0, 72, 33, 0, 35, 
  0, 34, 36, 0, 27, 12, 5, 0, 4, 98};

const int GedaElementGrammar::goto_default [] = {
  3, 2, 6, 40, 42, 41, 59, 8, 7, 12, 
  10, 15, 17, 20, 22, 24, 27, 29, 31, 33, 
  58, 57, 55, 49, 51, 53, 52, 105, 104, 103, 
  106, 67, 70, 56, 147, 146, 145, 148, 149, 150, 
  151, 152, 79, 127, 131, 158, 128, 165, 137, 54, 
  116, 115, 114, 117, 118, 119, 120, 90, 93, 95, 
  97, 139, 50, 87, 86, 85, 88, 48, 64, 63, 
  62, 65, 71, 73, 75, 77, 130, 66, 13, 0};

const int GedaElementGrammar::action_index [] = {
  17, -14, 27, 26, 25, 25, 33, -14, -14, -14, 
  2, -14, 12, -14, -14, 2, -14, 2, -14, -14, 
  8, -14, 8, -14, 8, -14, -14, 8, -14, 8, 
  -14, 8, -14, 25, -14, -14, 4, -14, 59, 59, 
  -14, -14, -14, -14, -14, -14, -14, -14, 40, -14, 
  47, -14, 39, -14, 43, -14, 44, -14, 59, 3, 
  0, 8, -14, -14, -14, 7, -14, 8, -14, -14, 
  8, 8, -14, 0, -14, 18, -14, 24, -14, -14, 
  -14, 22, -14, 24, 21, -14, -14, -14, 23, -14, 
  0, -14, -14, 21, -14, 13, -14, 16, -14, 15, 
  -14, 14, 16, -14, -14, -14, 11, 16, -14, -14, 
  20, -14, 16, 16, -14, -14, -14, 10, -14, -14, 
  -14, -10, -14, -9, -9, 0, 32, 0, 25, -14, 
  -14, 2, -14, -2, 25, -14, -14, -14, -14, -6, 
  -14, -3, -14, 0, 0, -14, -14, -14, -7, -14, 
  -14, -14, -14, 0, -14, -5, 32, -9, 2, -6, 
  5, -9, 2, -14, -12, 25, -14, -14, 25, -14, 
  -6, -14, -14, 1, -14, -14, -14, -4, -14, -14, 

  -80, -80, -80, -80, -66, 22, -80, -80, -80, -80, 
  -71, -80, -80, -80, -80, -68, -80, -69, -80, -80, 
  -73, -80, -74, -80, -76, -80, -80, -75, -80, -53, 
  -80, -55, -80, 38, -80, -80, -80, -80, -80, 62, 
  -80, -80, -80, -80, -80, -80, -80, -80, -80, -80, 
  -80, -80, -80, -80, -80, -80, -80, -80, 9, -80, 
  -80, -60, -80, -80, -80, -80, -80, -52, -80, -80, 
  -65, -54, -80, -51, -80, -56, -80, -63, -80, -80, 
  -80, -80, -80, -57, -34, -80, -80, -80, -80, -80, 
  -59, -80, -80, -58, -80, -61, -80, -13, -80, -80, 
  -80, -18, 5, -80, -80, -80, -80, 24, -80, -80, 
  -80, -80, -4, 6, -80, -80, -80, -80, -80, -80, 
  -80, -17, -80, -19, -22, 0, -49, -50, -31, -80, 
  -80, -29, -80, -10, 39, -80, -80, -80, -80, -2, 
  -80, -80, -80, -25, 13, -80, -80, -80, -80, -80, 
  -80, -80, -80, 25, -80, 12, 28, 8, -46, -9, 
  -80, -15, -12, -80, -67, 23, -80, -80, 31, -80, 
  -11, -80, -80, -80, -80, -80, -80, -80, -80, -80};

const int GedaElementGrammar::action_info [] = {
  11, 21, 21, 154, 178, 142, 21, 9, 0, 174, 
  11, 21, 37, 176, 11, 0, -68, 68, 1, 21, 
  122, 108, 35, 100, 21, 21, 179, 21, 111, 21, 
  82, 0, 21, 91, 5, 21, 4, 11, 9, 0, 
  39, 0, 38, 21, 11, 0, 102, 61, 101, 60, 
  113, 144, 112, 143, 84, 0, 83, 0, 0, 0, 
  0, 46, 47, 45, 44, 43, 0, 0, 0, 0, 
  0, 0, 0, 

  168, 26, 28, 25, 23, 16, 153, 19, 18, 166, 
  14, 81, 72, 107, 80, 175, 96, 133, 92, 94, 
  89, 78, 32, 74, 30, 69, 76, 132, 129, 98, 
  162, 36, 99, 167, 164, 110, 107, 171, 125, 172, 
  124, 123, 126, 89, 153, 135, 140, 138, 34, 136, 
  173, 134, 161, 121, 156, 96, 109, 155, 94, 141, 
  92, 0, 163, 121, 80, 0, 135, 138, 177, 138, 
  0, 157, 0, 89, 159, 0, 138, 80, 170, 169, 
  0, 0, 0, 89, 0, 132, 0, 0, 0, 80, 
  0, 0, 0, 0, 0, 0, 0, 0, 14, 14, 
  0, 69, 69, 0, 0, 160, 0, 166, 0, 138, 
  0, 0, 0, 0, 14, 14, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0};

const int GedaElementGrammar::action_check [] = {
  12, 11, 11, 10, 8, 8, 11, 13, -1, 8, 
  12, 11, 8, 10, 12, -1, 11, 10, 1, 11, 
  10, 10, 10, 8, 11, 11, 0, 11, 8, 11, 
  8, -1, 11, 10, 7, 11, 9, 12, 13, -1, 
  7, -1, 9, 11, 12, -1, 7, 7, 9, 9, 
  7, 7, 9, 9, 7, -1, 9, -1, -1, -1, 
  -1, 2, 3, 4, 5, 6, -1, -1, -1, -1, 
  -1, -1, -1, 

  46, 77, 77, 77, 77, 76, 31, 76, 76, 76, 
  76, 71, 77, 31, 77, 6, 77, 46, 77, 77, 
  77, 77, 77, 77, 77, 77, 77, 77, 77, 42, 
  45, 9, 66, 10, 46, 30, 31, 48, 60, 48, 
  59, 58, 42, 77, 31, 76, 48, 78, 10, 10, 
  37, 61, 44, 57, 42, 77, 32, 32, 77, 53, 
  77, -1, 77, 57, 77, -1, 76, 78, 6, 78, 
  -1, 43, -1, 77, 46, -1, 78, 77, 47, 48, 
  -1, -1, -1, 77, -1, 77, -1, -1, -1, 77, 
  -1, -1, -1, -1, -1, -1, -1, -1, 76, 76, 
  -1, 77, 77, -1, -1, 77, -1, 76, -1, 78, 
  -1, -1, -1, -1, 76, 76, -1, -1, -1, -1, 
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
  -1, -1};

