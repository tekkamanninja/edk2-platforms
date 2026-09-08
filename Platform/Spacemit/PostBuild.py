## @file
# PostBuild operations for Spacemit Platform.
#
# Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
# Copyright (c) 2025, Spacemit Corporation. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#

'''
PostBuild
'''

import os
import re
import sys
import signal
import argparse
import subprocess
import glob
import shutil
import configparser
import datetime

#
# Globals for help information
#
__prog__        = 'PostBuild'
__copyright__   = 'Copyright (c) 2025, Spacemit Corporation. All rights reserved.'
__description__ = 'Spacemit platform post-build operations.\n'

#
# Globals
#
gWorkspace = ''
gRootPath = ''
gConfigFilePath = ''
gArgs      = None

def LogAlways(Message):
    if isinstance(Message, str):
        sys.stdout.write (__prog__ + ': ' + Message + '\n')
        sys.stdout.flush()
    else:
        print (Message)

def Log(Message):
    global gArgs
    if not gArgs.Verbose:
        return
    if isinstance(Message, str):
        sys.stdout.write (__prog__ + ': ' + Message + '\n')
        sys.stdout.flush()
    else:
        print (Message)

def Error(Message, ExitValue=1):
    sys.stderr.write (__prog__ + ': ERROR: ' + Message + '\n')
    sys.exit (ExitValue)

def RelativePath(target):
    global gWorkspace
    Log('RelativePath' + target)
    return os.path.relpath (target, gWorkspace)

def NormalizePath(target):
    if isinstance(target, tuple):
        return os.path.normpath (os.path.join (*target))
    else:
        return os.path.normpath (target)

def RemoveFile(target):
    target = NormalizePath(target)
    if not target or target == os.pathsep:
        Error ('RemoveFile() invalid target')
    if os.path.exists(target):
        os.remove (target)
        Log ('remove %s' % (RelativePath (target)))

def RemoveDirectory(target):
    target = NormalizePath(target)
    if not target or target == os.pathsep:
        Error ('RemoveDirectory() invalid target')
    if os.path.exists(target):
        Log ('rmdir %s' % (RelativePath (target)))
        shutil.rmtree(target)

def CreateDirectory(target):
    target = NormalizePath(target)
    if os.path.isfile(target):
        target = os.path.dirname(target)
    if not os.path.exists(target):
        Log ('mkdir %s' % (RelativePath (target)))
        os.makedirs (target)

def Copy(src, dst):
    src = NormalizePath(src)
    dst = NormalizePath(dst)
    for File in glob.glob(src):
        Log ('copy %s -> %s' % (RelativePath (File), RelativePath (dst)))
        shutil.copy (File, dst)

def FindFile(directory, target):
    for root, _dirs, files in os.walk(directory):
        for file in files:
            if file == target:
                return os.path.join(root, file)

    LogAlways('File %s not found in %s' % (target, directory))
    return None

def GetPlatformConfig(config_file):
    """ Reads the platform specific config file

        param config_file: The configuration file that contains the
            post build operations and their parameters
        :type config_file: String
        :returns: The config defined in the the configuration file
        :rtype: Dictionary
    """
    config = {}
    configinfo = configparser.RawConfigParser()
    configinfo.optionxform = str
    configinfo.read(config_file)
    for section in configinfo.sections():
        config[section] = dict(configinfo.items(section))

    return config

def RunShellCommand(cmdstr):
	cmd_list = cmdstr.split()
	ret = subprocess.run(cmd_list)
	if ret.returncode:
		Error('Run command "%s" fail(%d)' % (cmdstr, ret.returncode))
	return ret.returncode

def GetIncludeBinFile (its_str):
    """Get include binary file name from the its config string.

    Args:
        its_str (string): string from the its file

	Returns:
		tuple: file name tuple.
    """
    # match pattern: keyword = /incbin/("filename")
    incbin_pattern = re.compile(r"\s*\w+\s*=\s*/incbin/\s*\(\s*\"(.+?)\"\s*\)")
    file_list = []
    for lines in its_str.splitlines():
        match_obj = incbin_pattern.match(lines)
        if match_obj:
            file_list.append(match_obj.groups()[0])

    return file_list

def BuildFitImage (config_dict):
    """Build flattened image tree file.

    Args:
        config_dict (dictionary): dictionary that include the image config info.

    Returns:
        boolean: TRUE: build success; FALSE: error happened
    """
    if 'FIT_ITS_FILE' not in config_dict:
        Error ("Please add its file for fit image")

    its_file = config_dict.get('FIT_ITS_FILE')
    build_para = config_dict.get('FIT_BUILD_FLAGS', '')
    board = config_dict.get('BOARD', 'UNKNOWN')
    fit_file = config_dict.get('FIT_Image_FILE', "%s.itb" % (board, ))
    fit_path = NormalizePath((gWorkspace, 'fitimage/%s' % (board, )))

    # delete the fit image that build before
    RemoveDirectory(fit_path)

    its_file = NormalizePath((gConfigFilePath, its_file))
    if not os.path.isfile(its_file):
        Error ("Its file(%s) NOT exist!" %its_file)

    with open(its_file, "r") as f:
        file_list = GetIncludeBinFile(f.read())

    # find file in its and copy it to fit image directory
    for file_name in file_list:
        new_file_path = NormalizePath((fit_path, os.path.dirname(file_name)))
        file_name = os.path.basename(file_name)
        file_path = FindFile(gWorkspace, file_name)
        if file_path:
            CreateDirectory(new_file_path)
            Copy(file_path, new_file_path)

    # copy its file to fit image directory, and change its path to fit image directory
    Copy(its_file, fit_path)
    fit_file = NormalizePath((fit_path, fit_file))
    its_file = NormalizePath((fit_path, os.path.basename(its_file)))

    LogAlways ('Generate fit image: {0}'.format (fit_file))
    cmdstr = "mkimage -f %s %s %s" %(its_file, build_para, fit_file)
    RunShellCommand(cmdstr)
    return True

def GetCmdArguments(config_dict):
    """ Get commandline inputs from user

        param config_dict: The environment variables to be
            used in the build process
        :type config_dict: Dictionary
        :returns: The commandline arguments input by the user
        :rtype: argparse object
    """
    class GetPackageFilePath(argparse.Action):
        """ Search and get the package file path
        """
        def __call__(self, parser, namespace, values, option_string = None):
            if values and config_dict.get("PACKAGES_PATH"):
                for path in config_dict.get("PACKAGES_PATH").split(':'):
                    filename = os.path.join(path, values)
                    if os.path.isfile(filename):
                        setattr(namespace, self.dest, filename)
                        return

    # get the build commands
    parser = argparse.ArgumentParser (
            prog = __prog__,
            description = __description__ + __copyright__,
            conflict_handler = 'resolve'
            )
    parser.add_argument (
            '-a', '--arch', dest = 'Arch', nargs = '+', action = 'append',
            required = True,
            help = '''ARCHS is one of list: IA32, X64, IPF, ARM, AARCH64 or EBC,
                    which overrides target.txt's TARGET_ARCH definition. To
                    specify more archs, please repeat this option.'''
            )
    parser.add_argument (
            '-v', '--verbose', dest = 'Verbose', action = 'store_true',
            help = '''Turn on verbose output with informational messages printed'''
            )
    parser.add_argument('-p', '--platform', dest = "platform_dsc_file", action = GetPackageFilePath,
            help = 'the platform description file path', required = True)
    parser.add_argument('--post', dest = "config_file", action = GetPackageFilePath,
            help = 'the post build configuration file', required = True)

    #
    # Parse command line arguments
    #
    global gArgs
    gArgs, remaining = parser.parse_known_args()
    gArgs.BuildType = 'all'
    for buildtype in ['all', 'fds', 'genc', 'genmake', 'clean', 'cleanall', 'modules', 'libraries', 'run']:
        if buildtype in remaining:
            gArgs.BuildType = buildtype
            remaining.remove(buildtype)
            break
    gArgs.Remaining = ' '.join(remaining)
    return gArgs

def KeyboardInterruption(int_signal, int_frame):
    """ Catches a keyboard interruption handler

        param int_signal: The signal this handler is called with
        :type int_signal: Signal
        param int_frame: The signal this handler is called with
        :type int_frame: frame
        :rtype: nothing
    """
    print("Signal #: {} Frame: {}".format(int_signal, int_frame))
    print("Quiting...")
    sys.exit(0)


if __name__ == '__main__':
    # post build config dictionary
    # keyword: related build operation function
    post_build_dict = {
		"FitImageBuild" : BuildFitImage,
	}

    # to quit the build
    signal.signal(signal.SIGINT, KeyboardInterruption)

    gRootPath = os.path.dirname(sys.argv[0])

    # get current environment variables
    build_config = os.environ.copy()
    gWorkspace = build_config.get('WORKSPACE', "./")

    # get command line parameters
    arguments = GetCmdArguments(build_config)
    if gArgs.BuildType == 'clean' or gArgs.BuildType == 'cleanall':
        sys.exit (0)

    # get platform specific config
    gConfigFilePath = os.path.dirname(arguments.config_file)
    platform_config = GetPlatformConfig(arguments.config_file)
    if not platform_config or platform_config.get("CONFIG") is None:
        Error ('Platform config file %s is empty' % (arguments.config_file, ))

    # update general build config with platform specific config
    config = platform_config.get("CONFIG")
    Log(config)

    for key in config:
        if key in post_build_dict and config[key] == 'TRUE':
            post_build_dict[key](config)
