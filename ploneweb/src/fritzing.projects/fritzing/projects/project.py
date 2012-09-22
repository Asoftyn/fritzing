from five import grok
from zope import schema
from plone.directives import form

from zope.interface import Invalid

from zope.schema import Int, Text
from plone.app.textfield import RichText
from plone.namedfile.field import NamedBlobFile, NamedBlobImage
from z3c.relationfield.schema import RelationList, RelationChoice
from plone.formwidget.contenttree import ObjPathSourceBinder

from fritzing.projects import FritzingProjectsMessageFactory as _
from fritzing.parts.part import IPart

def isFritzingSketch(file):
    """Constraint function to check for valid file name/type.
    """
    if file:
        if not file.filename.endswith(".fzz"):
            raise Invalid(
                _(u"Only accept Fritzing .fzz files")
            )
    return True

class IProject(form.Schema):
    """A Fritzing Project
    """
#    title = schema.ASCIILine(
#        title=_(u"Title"),
#        description=_(u"The title of your project"),
#    )
#    description = Text(
#        title=_(u"Teaser"),
#        description=_(u"Give a short, teasing description of your project"),
#    )
    body = RichText(
        title=_(u"Description"),
        description=_(u"Describe your project as detailed and structured as possible"),
        required=False,
    )
    picture = NamedBlobImage(
        title=_(u"Picture"),
        description=_(u"A picture or photo of your project"),
        required=False,
    )
    fritzingFile = NamedBlobFile(
        title=_(u"Fritzing Sketch"),
        description=_(u"The Fritzing .fzz file of your project"),
        constraint=isFritzingSketch,
    )
    parts = RelationList(
        title=_(u"Parts"),
        description=_(u"Parts used in this project"),
        value_type=RelationChoice(
                source=ObjPathSourceBinder(
                        object_provides=IPart.__identifier__
                    ),
            ),
        required=False,
    )
    form.omitted('numberOfViews')
    numberOfViews = Int(
        title = _(u"Views"),
        description = _(u"How often this project has been viewed"),
        default = 0
    )

class View(grok.View):
    """Default view (called "@@view"") for a project.
    
    The associated template is found in project_templates/view.pt.
    """
    
    grok.context(IProject)
    grok.require('zope2.View')
    grok.name('view')
    
    def update(self):
        """Called before rendering the template for this view
        """        
        self.context.numberOfViews += 1
    
