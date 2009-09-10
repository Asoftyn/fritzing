// This file was generated by qlalr - DO NOT EDIT!
#ifndef SVGPATHGRAMMAR_P_H
#define SVGPATHGRAMMAR_P_H

class SVGPathGrammar
{
public:
  enum {
    EOF_SYMBOL = 0,
    AITCH = 5,
    CEE = 7,
    COMMA = 11,
    EKS = 3,
    EL = 4,
    EM = 1,
    ESS = 8,
    KYU = 9,
    NUMBER = 12,
    TEE = 10,
    VEE = 6,
    WHITESPACE = 13,
    ZEE = 2,

    ACCEPT_STATE = 104,
    RULE_COUNT = 75,
    STATE_COUNT = 105,
    TERMINAL_COUNT = 14,
    NON_TERMINAL_COUNT = 41,

    GOTO_INDEX_OFFSET = 105,
    GOTO_INFO_OFFSET = 29,
    GOTO_CHECK_OFFSET = 29
  };

  static const char  *const spell [];
  static const int            lhs [];
  static const int            rhs [];

#ifndef QLALR_NO_SVGPATHGRAMMAR_DEBUG_INFO
  static const int     rule_index [];
  static const int      rule_info [];
#endif // QLALR_NO_SVGPATHGRAMMAR_DEBUG_INFO

  static const int   goto_default [];
  static const int action_default [];
  static const int   action_index [];
  static const int    action_info [];
  static const int   action_check [];

  static inline int nt_action (int state, int nt)
  {
    const int *const goto_index = &action_index [GOTO_INDEX_OFFSET];
    const int *const goto_check = &action_check [GOTO_CHECK_OFFSET];

    const int yyn = goto_index [state] + nt;

    if (yyn < 0 || goto_check [yyn] != nt)
      return goto_default [nt];

    const int *const goto_info = &action_info [GOTO_INFO_OFFSET];
    return goto_info [yyn];
  }

  static inline int t_action (int state, int token)
  {
    const int yyn = action_index [state] + token;

    if (yyn < 0 || action_check [yyn] != token)
      return - action_default [state];

    return action_info [yyn];
  }
};


#endif // SVGPATHGRAMMAR_P_H

