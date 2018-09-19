"""
Load a hierarchical configuration file.
"""

import os
import libconf

class Configuration:
    """
    The hierarchy set up via the "subconfig" call.

    It first searches for a section within the current configuration section
    named the subconfig, if it is not found, it will search for a file named as
    the subconfig, to load this
    Together with @include within the config file this enables superb
    de-duplication within sensor nodes, by @include-ing the common sensor config,
    to arrange them in groups, as well as configuring settings that are common
    among all sensors within the main configuration.
    """

    def __init__(self,
                 masterconfig=None,
                 subconfig=None,
                 configfile=None,
                 fallback=None):
        if (not masterconfig and not configfile) or \
           (masterconfig and configfile):
            raise ValueError("Must set exactly one of masterconfig and configfile")

        self.filename = configfile

        if configfile:
            self.fallback = None
            with open(configfile) as cfgf:
                self.config = libconf.load(cfgf)

        if masterconfig:
            self.fallback = masterconfig
            if subconfig:
                self.config = masterconfig[subconfig]
            else:
                self.config = masterconfig
        else:
            self.fallback = fallback

        self.__subconfigname = subconfig
        if not self.filename and self.fallback:
            self.filename = self.fallback.filename

        if self.__subconfigname:
            self.__name = "{}:{}".format(self.fallback.__name, self.__subconfigname)
        elif self.filename:
            self.__name = self.filename
        else:
            self.__name = "ERROR"
            raise KeyError("Fuck you")

    def subconfig(self, name):
        """
        Load a hierarchical lower configuration file
        """

        if name in self.config:
            return Configuration(masterconfig=self,
                                 subconfig=name)

        path = os.path.join(self["nodeconfigpath"], name + ".conf")
        print("Loading new config file: ", path)
        return Configuration(configfile=path,
                             fallback=self)

    def get_item(self, key, stack=None):
        """
        Cascate through the tree, trying to find the config item

        The "stack" argument is used to display more conclusive error messages
        if a config element is not found.
        """
        if stack is None:
            stack = [self.__name]
        else:
            stack.append(self.__name)

        try:
            return self.config[key]
        except KeyError:
            pass
        if self.fallback:
            return self.fallback.get_item(key, stack)
        else:
            raise KeyError("{} in {}".format(key, " <= ".join(stack)))

    def __getitem__(self, key):
        """
        implement []-accessor
        """
        return self.get_item(key)
