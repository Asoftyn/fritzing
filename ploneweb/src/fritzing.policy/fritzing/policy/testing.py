from plone.app.testing import PloneSandboxLayer
from plone.app.testing import applyProfile
from plone.app.testing import PLONE_FIXTURE
from plone.app.testing import IntegrationTesting

from zope.configuration import xmlconfig

class FritzingPolicy(PloneSandboxLayer):

    defaultBases = (PLONE_FIXTURE,)
    
    def setUpZope(self, app, configurationContext):
        # Load ZCML:
        import fritzing.policy
        xmlconfig.file('configure.zcml',
            fritzing.policy,
            context=configurationContext
        )
        
    def setUpPloneSite(self, portal):
        applyProfile(portal, 'fritzing.policy:default')
        
FRITZING_POLICY_FIXTURE = FritzingPolicy()
FRITZING_POLICY_INTEGRATION_TESTING = IntegrationTesting(
    bases=(FRITZING_POLICY_FIXTURE,),
    name="Fritzing:Integration"
)
