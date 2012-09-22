import unittest2 as unittest
from fritzing.policy.testing import FRITZING_POLICY_INTEGRATION_TESTING

class TestSetup(unittest.TestCase):
    
    layer = FRITZING_POLICY_INTEGRATION_TESTING
    
    def test_portal_title(self):
        portal = self.layer['portal']
        self.assertEqual(
            "Fritzing",
            portal.getProperty('title')
        )


    def test_portal_description(self):
        portal = self.layer['portal']
        self.assertEqual(
            "From prototype to product",
            portal.getProperty('description')
        )

