/*
 * pstate-frequency Easier control of the Intel p-state driver
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
 *
 * For questions please contact pyamsoft at pyam.soft@gmail.com
 */

#include <iostream>
#include <cstdlib>

#include <getopt.h>
#include <unistd.h>

#include "include/psfreq_color.h"
#include "include/psfreq_cpu.h"
#include "include/psfreq_cpuvalues.h"
#include "include/psfreq_logger.h"
#include "include/psfreq_util.h"

void setCpuValues(const psfreq::cpu &cpu, const psfreq::cpuValues &cpuValues);
void printCpuValues(const psfreq::cpu& cpu);
void printRealtimeFrequency(const psfreq::cpu& cpu);
void printGPL();
void printVersion();
void printHelp();
int handleOptionResult(psfreq::cpu &cpu, psfreq::cpuValues &cpuValues, const int result);
int planFromOptArg(char *const arg);
const std::string governorFromOptArg(char *const arg, std::vector<std::string> availableGovernors);

void setCpuValues(const psfreq::cpu &cpu, const psfreq::cpuValues &cpuValues)
{
	std::ostringstream log;
	if (psfreq::logger::isDebug()) {
		log << "pstate-frequency [main.cpp]: setCpuValues" << std::endl;
		psfreq::logger::d(log);
	}

	const int max = cpuValues.getMax();
	if (psfreq::logger::isDebug()) {
		log << "\tMax: " << max << std::endl;
		psfreq::logger::d(log);
	}

	const int min = cpuValues.getMin();
	if (psfreq::logger::isDebug()) {
		log << "\tMin: "<< min << std::endl;
		psfreq::logger::d(log);
	}

	const int turbo = cpuValues.getTurbo();
	if (psfreq::logger::isDebug()) {
		log << "\tTurbo: "<< turbo << std::endl;
		psfreq::logger::d(log);
	}

	const std::string governor = cpuValues.getGovernor();
	if (psfreq::logger::isDebug()) {
		log << "\tGovernor: "<< governor << std::endl;
		log << "\tProcessing new CPU values..." << std::endl;
		psfreq::logger::d(log);
	}

	int newTurbo = (turbo != -1 ? turbo : cpu.getTurboBoost());
	newTurbo = psfreq::boundValue(newTurbo, 0, 1);
	if (psfreq::logger::isDebug()) {
		log << "\tnewTurbo: "<< newTurbo << std::endl;
		psfreq::logger::d(log);
	}

	int newMin = (min >= 0 ? min : cpu.getMinPState());
	newMin = psfreq::boundValue(newMin, cpu.getInfoMinValue(), cpu.getInfoMaxValue() - 1);
	if (psfreq::logger::isDebug()) {
		log << "\tnewMin: "<< newMin << std::endl;
		psfreq::logger::d(log);
	}

	int newMax = (max >= 0 ? max : cpu.getMaxPState());
	newMax = psfreq::boundValue(newMax, cpu.getInfoMinValue() + 1, cpu.getInfoMaxValue());
	newMax = (newMax > newMin ? newMax : newMin + 1);
	if (psfreq::logger::isDebug()) {
		log << "\tnewMax: " << newMax << std::endl;
		psfreq::logger::d(log);
	}

	const std::string newGovernor = (governor != "" ? governor : cpu.getGovernor());
	if (psfreq::logger::isDebug()) {
		log << "\tnewGovernor: "<< newGovernor << std::endl;
		psfreq::logger::d(log);
	}

	if (psfreq::logger::isDebug()) {
		log << "\tSetting sane defaults" << std::endl;
		psfreq::logger::d(log);
	}
	cpu.setSaneDefaults();

	if (psfreq::logger::isDebug()) {
		log << "\tSetting new values" << std::endl;
		psfreq::logger::d(log);
	}
	cpu.setScalingMax(newMax);
	cpu.setScalingMin(newMin);
	cpu.setTurboBoost(newTurbo);
	cpu.setGovernor(newGovernor);
}

int planFromOptArg(char *const arg)
{
	const std::string convertedArg(arg);
	int plan;
	std::ostringstream log;
	if (psfreq::logger::isDebug()) {
		log << "pstate-frequency [main.cpp]: planFromOptArg" << std::endl;
		psfreq::logger::d(log);
	}
	if (convertedArg.compare("1") == 0 || psfreq::stringStartsWith("powersave", convertedArg)) {
		plan = 1;
		if (psfreq::logger::isDebug()) {
			log << "\tPlan: powersave 1" << std::endl;
		}
	} else if (convertedArg.compare("2") == 0 || psfreq::stringStartsWith("performance", convertedArg)) {
		plan = 2;
		if (psfreq::logger::isDebug()) {
			log << "\tPlan: performance 2" << std::endl;
		}
	} else if (convertedArg.compare("3") == 0 || psfreq::stringStartsWith("max-performance", convertedArg)) {
		plan = 3;
		if (psfreq::logger::isDebug()) {
			log << "\tPlan: max-performance 3" << std::endl;
		}
#ifdef INCLUDE_UDEV_RULE
#if INCLUDE_UDEV_RULE == 1
	} else if (convertedArg.compare("0") == 0 || psfreq::stringStartsWith("auto", convertedArg)) {
		plan = 0;
		if (psfreq::logger::isDebug()) {
			log << "\tPlan: auto 0" << std::endl;
		}
#endif
#endif
	} else {
		if (!psfreq::logger::isAllQuiet()) {
			std::ostringstream err;
			err << psfreq::PSFREQ_COLOR_BOLD_RED << "Invalid plan: "
				<< convertedArg << psfreq::PSFREQ_COLOR_OFF << std::endl;
			psfreq::logger::e(err.str());
		}
		exit(EXIT_FAILURE);
	}
	if (psfreq::logger::isDebug()) {
		psfreq::logger::d(log);
	}
	return plan;
}

void printRealtimeFrequency(const psfreq::cpu& cpu)
{
	if (psfreq::logger::isNormal() || psfreq::logger::isDebug()) {
		printVersion();
		const std::vector<std::string> frequencies = cpu.getRealtimeFrequencies();
		std::ostringstream oss;
		for (unsigned int i = 0; i < cpu.getNumber(); ++i) {
			std::string freq = frequencies[i];
			oss << psfreq::PSFREQ_COLOR_BOLD_WHITE
				<< "    pstate::" << psfreq::PSFREQ_COLOR_BOLD_GREEN
				<< "CPU[" << psfreq::PSFREQ_COLOR_BOLD_MAGENTA << i
				<< psfreq::PSFREQ_COLOR_BOLD_GREEN << "]  -> "
				<< psfreq::PSFREQ_COLOR_BOLD_CYAN
				<< freq.substr(0, freq.size() - 1) << "MHz"
				<< psfreq::PSFREQ_COLOR_OFF << std::endl;
			psfreq::logger::n(oss);
		}
	}
}

const std::string governorFromOptArg(char *const arg, std::vector<std::string> availableGovernors)
{
	const std::string convertedArg(arg);
	std::ostringstream gov;
	std::string governor = "";
	std::ostringstream log;
	if (psfreq::logger::isDebug()) {
		log << "pstate-frequency [main.cpp]: governorFromOptArg" << std::endl;
		psfreq::logger::d(log);
	}

	for (unsigned int i = 0; i < availableGovernors.size(); ++i) {
		if (psfreq::logger::isDebug()) {
			log << "\tComparing argument: " << convertedArg << " to governor: "
				<< availableGovernors[i] << std::endl;
			psfreq::logger::d(log);
		}
		if (psfreq::stringStartsWith(availableGovernors[i], convertedArg)) {
			governor = availableGovernors[i];
			break;
		}
	}
	if (governor == "") {
		if (!psfreq::logger::isAllQuiet()) {
			std::ostringstream oss;
			oss << psfreq::PSFREQ_COLOR_BOLD_RED << "Invalid governor: "
				<< psfreq::PSFREQ_COLOR_BOLD_WHITE << convertedArg << std::endl;
			oss << psfreq::PSFREQ_COLOR_BOLD_RED << "Valid governors are: [ " << psfreq::PSFREQ_COLOR_BOLD_WHITE;
			for (unsigned int i = 0; i < availableGovernors.size(); ++i) {
				oss << availableGovernors[i] << " ";
			}
			oss << psfreq::PSFREQ_COLOR_BOLD_RED << "]" << psfreq::PSFREQ_COLOR_OFF << std::endl;
			psfreq::logger::e(oss.str());
		}
		exit(EXIT_FAILURE);
	}
	if (psfreq::logger::isDebug()) {
		log << "\tGovernor: " << governor << std::endl;
		psfreq::logger::d(log);
	}
	return governor;
}

void printGPL()
{
	if (psfreq::logger::isNormal() || psfreq::logger::isDebug()) {
		std::ostringstream oss;
		oss << "pstate-frequency comes with ABSOLUTELY NO WARRANTY."
			<< std::endl
			<< "This is free software, and you are welcome to redistribute it"
			<< std::endl << "under certain conditions." << std::endl
			<< "Please see the README for details."
			<< psfreq::PSFREQ_COLOR_OFF << std::endl << std::endl;
		psfreq::logger::n(oss.str());
	}
}

void printVersion()
{
	if (psfreq::logger::isNormal() || psfreq::logger::isDebug()) {
		std::ostringstream oss;
		oss << std::endl;
#ifdef VERSION
		oss << psfreq::PSFREQ_COLOR_BOLD_BLUE << "pstate-frequency  "
			<< psfreq::PSFREQ_COLOR_BOLD_MAGENTA << VERSION << psfreq::PSFREQ_COLOR_OFF
			<< std::endl;
#endif
		psfreq::logger::n(oss.str());
	}
}

void printCpuValues(const psfreq::cpu& cpu)
{
	if (psfreq::logger::isNormal() || psfreq::logger::isDebug()) {
		printVersion();
		std::ostringstream oss;
		oss << psfreq::PSFREQ_COLOR_BOLD_WHITE
			<< "    pstate::" << psfreq::PSFREQ_COLOR_BOLD_GREEN << "CPU_DRIVER     -> "
			<< psfreq::PSFREQ_COLOR_BOLD_CYAN << cpu.getDriver() << std::endl;
		oss << psfreq::PSFREQ_COLOR_BOLD_WHITE
			<< "    pstate::" << psfreq::PSFREQ_COLOR_BOLD_GREEN << "CPU_GOVERNOR   -> "
			<< psfreq::PSFREQ_COLOR_BOLD_CYAN << cpu.getGovernor() << std::endl;
		const int turbo = cpu.getTurboBoost();
		oss << psfreq::PSFREQ_COLOR_BOLD_WHITE
			<< "    pstate::" << psfreq::PSFREQ_COLOR_BOLD_GREEN << "NO_TURBO       -> "
			<< psfreq::PSFREQ_COLOR_BOLD_CYAN << turbo << " : "
			<< (psfreq::hasPstate() ? (turbo == 1 ? "OFF" : "ON")
					: (turbo == 1 ? "ON" : "OFF")) << std::endl;
		oss << psfreq::PSFREQ_COLOR_BOLD_WHITE
			<< "    pstate::" << psfreq::PSFREQ_COLOR_BOLD_GREEN << "CPU_MIN        -> "
			<< psfreq::PSFREQ_COLOR_BOLD_CYAN << cpu.getMinPState() << "% : "
			<< static_cast<int>(cpu.getScalingMinFrequency()) << "KHz" << std::endl;
		oss << psfreq::PSFREQ_COLOR_BOLD_WHITE
			<< "    pstate::" << psfreq::PSFREQ_COLOR_BOLD_GREEN << "CPU_MAX        -> "
			<< psfreq::PSFREQ_COLOR_BOLD_CYAN << cpu.getMaxPState() << "% : "
			<< static_cast<int>(cpu.getScalingMaxFrequency()) << "KHz" << std::endl;
		oss << psfreq::PSFREQ_COLOR_OFF;
		psfreq::logger::n(oss.str());
	}
}

void printHelp()
{
	if (psfreq::logger::isNormal() || psfreq::logger::isDebug()) {
		std::ostringstream oss;
		oss << "usage:"
			<< "pstate-frequency [verbose] [action] [option(s)]"
			<< std::endl
			<< "    verbose:" << std::endl
			<< "        unprivilaged:" << std::endl
			<< "            -d | --debug     Print debugging messages to stdout" << std::endl
			<< "            -q | --quiet     Supress all non-error output" << std::endl
			<< "            -a | --all-quiet Supress all output" << std::endl
			<< std::endl
			<< "    actions:" << std::endl
			<< "        unprivilaged:" << std::endl
			<< "            -h | --help      Display this help and exit" << std::endl
			<< "            -v | --version   Display application version and exit" << std::endl
			<< "            -g | --get       Access current CPU values" << std::endl
			<< "        privilaged:" << std::endl
			<< "            -s | --set       Modify current CPU values" << std::endl
			<< std::endl
			<< "    options:" << std::endl
			<< "        unprivilaged:" << std::endl
			<< "            -c | --current   Display the current user set CPU values" << std::endl
			<< "            -r | --real      Display the real time CPU frequencies" << std::endl
			<< "        privilaged: "<< std::endl
			<< "            -p | --plan      Set a predefined power plan" << std::endl
			<< "            -m | --max       Modify current CPU max frequency" << std::endl
			<< "            -o | --gov       Set the cpufreq governor" << std::endl
			<< "            -n | --min       Modify current CPU min frequency" << std::endl
			<< "            -t | --turbo     Modify curent CPU turbo boost state" << std::endl;
		psfreq::logger::n(oss.str());
	}
}

int handleOptionResult(psfreq::cpu &cpu, psfreq::cpuValues &cpuValues, const int result)
{
	switch(result) {
	case 0:
                return 0;
        case 'h':
		printGPL();
		printHelp();
                return -1;
        case 'c':
		cpuValues.setRequested(0);
		return 0;
        case 'r':
		cpuValues.setRequested(1);
		return 0;
        case 'v':
		printGPL();
		printVersion();
                return -1;
        case 's':
		cpuValues.setAction(1);
                return 0;
        case 'd':
		psfreq::logger::setDebug();
                return 0;
	case 'a':
		psfreq::logger::setAllQuiet();
		return 0;
	case 'q':
		psfreq::logger::setQuiet();
		return 0;
        case 'g':
		cpuValues.setAction(0);
                return 0;
        case 'p':
		cpuValues.setPlan(planFromOptArg(optarg));
                return 0;
        case 'm':
		cpuValues.setMax(psfreq::stringToNumber(optarg));
                return 0;
	case 'o':
		cpuValues.setGovernor(governorFromOptArg(optarg, cpu.getAvailableGovernors()));
		return 0;
        case 'n':
		cpuValues.setMin(psfreq::stringToNumber(optarg));
		return 0;
        case 't':
		cpuValues.setTurbo(psfreq::stringToNumber(optarg));
		return 0;
	}
	return EXIT_FAILURE;
}

int main(int argc, char** argv)
{
	psfreq::cpu cpu = psfreq::cpu();
	psfreq::cpuValues cpuValues = psfreq::cpuValues();

	int finalOptionResult = 0;
	int optionResult = 0;
	const char *const shortOptions = "hvcrsdagqp:m:n:t:o:";
	struct option longOptions[] = {
                {"help",          no_argument,        NULL,           'h'},
                {"version",       no_argument,        NULL,           'v'},
                {"get",           no_argument,        NULL,           'g'},
                {"set",           no_argument,        NULL,           's'},
                {"current",       no_argument,        NULL,           'c'},
                {"real",          no_argument,        NULL,           'r'},
                {"quiet",         no_argument,        NULL,           'q'},
                {"all-quiet",     no_argument,        NULL,           'a'},
                {"debug",         no_argument,        NULL,           'd'},
                {"plan",          required_argument,  NULL,           'p'},
                {"gov",           required_argument,  NULL,           'o'},
		{"max",		  required_argument,  NULL,           'm'},
		{"min",		  required_argument,  NULL,           'n'},
		{"turbo",	  required_argument,  NULL,           't'},
		{0,		  0,                  0,              0}
                };

	while (true) {
                int index = 0;
                optionResult = getopt_long(argc, argv, shortOptions, longOptions, &index);
                if (optionResult == -1) {
                        break;
                } else {
			finalOptionResult = handleOptionResult(cpu, cpuValues, optionResult);
                        if (finalOptionResult == -1) {
                                return EXIT_SUCCESS;
                        } else if (finalOptionResult == EXIT_FAILURE) {
                                return EXIT_FAILURE;
                        }
                }
        }

	if (cpuValues.isActionNull()) {
		printGPL();
		printHelp();
		return EXIT_SUCCESS;
	} else if (cpuValues.isActionGet()) {
		if (cpuValues.getRequested() == 0) {
			printCpuValues(cpu);
		} else {
			printRealtimeFrequency(cpu);
		}
	} else {
		if (geteuid() == 0) {
			if (cpuValues.isInitialized()) {
				setCpuValues(cpu, cpuValues);
				printCpuValues(cpu);
			} else {
				if (!psfreq::logger::isAllQuiet()) {
					std::ostringstream oss;
					oss << psfreq::PSFREQ_COLOR_BOLD_RED
						<< "Set called with no target"
						<< psfreq::PSFREQ_COLOR_OFF << std::endl;
					psfreq::logger::e(oss.str());
				}
				return EXIT_FAILURE;
			}
		} else {
			if (!psfreq::logger::isAllQuiet()) {
				std::ostringstream oss;
				oss << psfreq::PSFREQ_COLOR_BOLD_RED << "Root privilages required"
					<< psfreq::PSFREQ_COLOR_OFF << std::endl;
				psfreq::logger::e(oss.str());
			}
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
