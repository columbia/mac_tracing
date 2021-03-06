//===-- CommandAlias.h -----------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_CommandAlias_h_
#define liblldb_CommandAlias_h_

// C Includes
// C++ Includes
#include <memory>

// Other libraries and framework includes
// Project includes
#include "lldb/Interpreter/Args.h"
#include "lldb/Interpreter/CommandObject.h"
#include "lldb/lldb-forward.h"

namespace lldb_private {
class CommandAlias : public CommandObject {
public:
  typedef std::unique_ptr<CommandAlias> UniquePointer;

  CommandAlias(CommandInterpreter &interpreter, lldb::CommandObjectSP cmd_sp,
               const char *options_args, const char *name,
               const char *help = nullptr, const char *syntax = nullptr,
               uint32_t flags = 0);

  void GetAliasExpansion(StreamString &help_string);

  bool IsValid() { return m_underlying_command_sp && m_option_args_sp; }

  explicit operator bool() { return IsValid(); }

  bool WantsRawCommandString() override;

  bool WantsCompletion() override;

  int HandleCompletion(Args &input, int &cursor_index,
                       int &cursor_char_position, int match_start_point,
                       int max_return_elements, bool &word_complete,
                       StringList &matches) override;

  int HandleArgumentCompletion(Args &input, int &cursor_index,
                               int &cursor_char_position,
                               OptionElementVector &opt_element_vector,
                               int match_start_point, int max_return_elements,
                               bool &word_complete,
                               StringList &matches) override;

  Options *GetOptions() override;

  bool IsAlias() override { return true; }

  bool IsDashDashCommand() override;

  const char *GetHelp() override;

  const char *GetHelpLong() override;

  void SetHelp(const char *str) override;

  void SetHelpLong(const char *str) override;

  bool Execute(const char *args_string, CommandReturnObject &result) override;

  lldb::CommandObjectSP GetUnderlyingCommand() {
    return m_underlying_command_sp;
  }
  OptionArgVectorSP GetOptionArguments() { return m_option_args_sp; }
  const char *GetOptionString() { return m_option_string.c_str(); }

  // this takes an alias - potentially nested (i.e. an alias to an alias)
  // and expands it all the way to a non-alias command
  std::pair<lldb::CommandObjectSP, OptionArgVectorSP> Desugar();

protected:
  bool IsNestedAlias();

private:
  lldb::CommandObjectSP m_underlying_command_sp;
  std::string m_option_string;
  OptionArgVectorSP m_option_args_sp;
  LazyBool m_is_dashdash_alias;
  bool m_did_set_help : 1;
  bool m_did_set_help_long : 1;
};
} // namespace lldb_private

#endif // liblldb_CommandAlias_h_
