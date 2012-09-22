from five import grok
from zope import schema
from plone.directives import form

from plone.app.textfield import RichText

from fritzing.parts import FritzingPartsMessageFactory as _

# View
from plone.memoize.instance import memoize
from fritzing.parts.part import IPart
from Products.CMFCore.utils import getToolByName

class IPartFolder(form.Schema):
    """A folder that can contain parts
    """
    
    text = RichText(
            title=_(u"Body text"),
            description=_(u"Introductory text for this part folder"),
            required=False
        )

class View(grok.View):
    """Default view (called "@@view"") for a part folder.
    
    The associated template is found in partfolder_templates/view.pt.
    """
    
    grok.context(IPartFolder)
    grok.require('zope2.View')
    grok.name('view')
    
    def update(self):
        """Called before rendering the template for this view
        """        
        self.haveParts = len(self.parts()) > 0
    
    @memoize
    def parts(self):
        """Get all child parts.
        """
        catalog = getToolByName(self.context, 'portal_catalog')

        return [ dict(url=part.getURL(),
                      title=part.Title,
                      description=part.Description,)
                 for part in 
                    catalog({'object_provides': IPart.__identifier__,
                             'path': dict(query='/'.join(self.context.getPhysicalPath()),
                                      depth=1),
                             'sort_on': 'sortable_title'})
               ]

