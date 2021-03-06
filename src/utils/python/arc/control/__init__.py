from __future__ import absolute_import

#
# arcctl controllers for all installations
#

# arcctl deploy
from .ThirdPartyDeployment import ThirdPartyControl
# arcctl test-ca
from .TestCA import TestCAControl

CTL_COMPONENTS = [
    ThirdPartyControl,
    TestCAControl
]

#
# arcctl controllers for A-REX
#

# arcctl config
try:
    from .Config import ConfigControl
except ImportError:
    pass
else:
    CTL_COMPONENTS.append(ConfigControl)

# arcclt service
try:
    from .Services import ServicesControl
except ImportError:
    pass
else:
    CTL_COMPONENTS.append(ServicesControl)

# arcctl rte
try:
    from .RunTimeEnvironment import RTEControl
except ImportError:
    pass
else:
    CTL_COMPONENTS.append(RTEControl)

# arcctl accounting
try:
    from .Accounting import AccountingControl
except ImportError:
    pass
else:
    CTL_COMPONENTS.append(AccountingControl)

# arcctl job
try:
    from .Jobs import JobsControl
except ImportError:
    pass
else:
    CTL_COMPONENTS.append(JobsControl)

# arcctl cache
try:
    from .Cache import CacheControl
except ImportError:
    pass
else:
    CTL_COMPONENTS.append(CacheControl)
