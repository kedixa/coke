module;

// clang-format off

#include "coke/tools/option_parser.h"
#include "coke/tools/scope.h"

export module coke.tools;

export namespace coke {

// tools/option_parser.h

using coke::NULL_SHORT_NAME;
using coke::NULL_LONG_NAME;

using coke::OptionError;
using coke::DuplicateOptionError;
using coke::InvalidOptionError;
using coke::InvalidValueError;
using coke::NoSuchOptionError;
using coke::UnexpectedValueError;
using coke::OptionNotSetError;
using coke::ValidateError;

using coke::IntegerOptionTrait;
using coke::FloatingOptionTrait;
using coke::FlagOptionTrait;
using coke::CountableOptionTrait;
using coke::BoolOptionTrait;
using coke::StringOptionTrait;
using coke::DataUnitTrait;
using coke::MultiOptionTrait;

using coke::OptionBase;
using coke::BasicOption;;
using coke::IntegerOption;
using coke::MIntegerOption;
using coke::FloatingOption;
using coke::MFloatingOption;
using coke::FlagOption;;
using coke::CountableOption;;
using coke::BoolOption;;
using coke::StringOption;;
using coke::MStringOption;;
using coke::DataUnitOption;

using coke::OptionParser;

// tools/scope.h

using coke::ScopeExit;

} // namespace coke
