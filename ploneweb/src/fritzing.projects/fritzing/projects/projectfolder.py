from five import grok
from zope import schema
from plone.directives import form

from plone.app.textfield import RichText

from fritzing.projects import FritzingProjectsMessageFactory as _

# View
from plone.memoize.instance import memoize
from fritzing.projects.project import IProject
from Products.CMFCore.utils import getToolByName

class IProjectFolder(form.Schema):
    """A folder that can contain projects
    """
    
    text = RichText(
            title=_(u"Body text"),
            description=_(u"Introductory text for this project folder"),
            required=False
        )

class View(grok.View):
    """Default view (called "@@view"") for a project folder.
    
    The associated template is found in projectfolder_templates/view.pt.
    """
    
    grok.context(IProjectFolder)
    grok.require('zope2.View')
    grok.name('view')
    
    def update(self):
        """Called before rendering the template for this view
        """        
        self.haveProjects       = len(self.projects()) > 0
    
    @memoize
    def projects(self):
        """Get all child projects.
        """
        catalog = getToolByName(self.context, 'portal_catalog')

        return [ dict(url=project.getURL(),
                      title=project.Title,
                      description=project.Description,)
                 for project in 
                    catalog({'object_provides': IProject.__identifier__,
                             'path': dict(query='/'.join(self.context.getPhysicalPath()),
                                      depth=1),
                             'sort_on': 'sortable_title'})
               ]

