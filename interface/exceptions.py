# Definition for exceptions

class WaschError(Exception):
    pass


class NodeStateError(WaschError):
    pass


class MasterCommandError(WaschError):
    pass
