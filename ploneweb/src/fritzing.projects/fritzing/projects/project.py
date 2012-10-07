from five import grok
from zope import schema
from plone.directives import form, dexterity
from zope.interface import Invalid
from zope.schema import ASCII, ASCIILine, Int, SourceText
from plone.app.textfield import RichText
from plone.namedfile.field import NamedBlobFile, NamedBlobImage
from z3c.relationfield.schema import RelationList, RelationChoice
from plone.formwidget.contenttree import ObjPathSourceBinder
from zope.lifecycleevent.interfaces import IObjectCreatedEvent, IObjectModifiedEvent
from Products.CMFCore.utils import getToolByName
from zope.app.component.hooks import getSite

from Acquisition import aq_inner
import zipfile, xml.dom.minidom, xml.dom
from cStringIO import StringIO

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
    form.omitted('breadboardView')
    breadboardView = ASCII(
        title=_(u"Breadboard View"),
        description=_(u"The SVG graphics for the breadboard view"),
    )
    form.omitted('schematicView')
    schematicView = ASCII(
        title=_(u"Schematic View"),
        description=_(u"The SVG graphics for the schematic view"),
    )
    form.omitted('pcbView')
    pcbView = ASCII(
        title=_(u"PCB View"),
        description=_(u"The SVG graphics for the pcb view"),
    )
    form.omitted('code')
    code = SourceText(
        title=_(u"Code"),
        description=_(u"The embedded microcontroller code"),
    )
    form.omitted('parts')
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
    form.omitted('customParts')
    customParts = RelationList(
        title=_(u"Custom Parts"),
        description=_(u"Custom parts used in this project"),
        value_type=RelationChoice(
                source=ObjPathSourceBinder(
                        object_provides=IPart.__identifier__
                    ),
            ),
        required=False,
    )
    form.omitted('fritzingVersion')
    fritzingVersion = ASCIILine(
        title=_(u"Fritzing Version"),
        description=_(u"The Fritzing version that this part was created with."),
    )
    form.omitted('numberOfDownloads')
    numberOfDownloads = Int(
        title = _(u"Downloads"),
        description = _(u"How often this project has been downloaded"),
        default = 0
    )
    form.omitted('numberOfViews')
    numberOfViews = Int(
        title = _(u"Views"),
        description = _(u"How often this project has been viewed"),
        default = 0
    )

class View(dexterity.DisplayForm):
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


@grok.subscribe(IProject, IObjectCreatedEvent)
@grok.subscribe(IProject, IObjectModifiedEvent)
def extractFZZ(project, event):
    fileData = StringIO(project.fritzingFile.data)
    zf = None
    try:
        zf = zipfile.ZipFile(fileData) 
    except:
        raise Invalid(
            _(u"This fzz file seems corrupted.")
        )
        
    customPartFileNames = []
    codeFileNames = []
    fzXML = None
    for i, name in enumerate(zf.namelist()):   
        if name.endswith('.fz'):
            fzXML = zf.read(name)
        elif name.endswith('.fzp'):
            customPartFileNames.append(name)
        elif name.endswith('.fzz'):
            continue
            # ignore: artefact from renaming
        elif name.endswith('.svg'):
            continue
            # TODO: custom part or board graphics
        else:
            codeFileNames.append(name) 
            # TODO: this should be checked against those listed in the .fz
    if not fzXML:
        raise Invalid(
            _(u"No Fritzing sketch (.fz) found in fzz.")
        )
    
    # TODO Generate SVGs
    #~ breadboardSVG = None
    #~ schematicSVG = None
    #~ pcbSVG = None
    # Run Fritzing to load this fzz and export all SVGs
    project.breadboardView = ""
    project.schematicView = ""
    project.pcbView = ""
    
    # Parse FZ
    dom = None
    try:
        dom = xml.dom.minidom.parseString(fzXML)
    except:
        raise Invalid(
            _(u"The fz is not a valid XML file.")
        )
    if dom == None:
        raise Invalid(
            _(u"The fz is not a valid XML file (2).")
        )
    root = dom.documentElement
    if root.tagName != 'module':
        raise Invalid(
            _(u"Missing required 'module' root element in fzp.")
        )

    project.fritzingVersion = root.getAttribute('fritzingVersion')
    
    # Parse parts
    project.parts = []
    instances = root.getElementsByTagName('instance')
    partModuleIds = dict()
    ignoreIds = ('WireModuleId', 
        'LogoImageModuleID', 
        'TwoLayerRectanglePCBModuleID',
        '423120090505', 
        '423120090505_2')
    catalog = getToolByName(getSite(), 'portal_catalog')
    for instance in instances:
        partModuleId = instance.getAttribute('moduleIdRef')
        if (partModuleId not in ignoreIds):
            if (not partModuleIds.has_key(partModuleId )):
                partModuleIds[partModuleId ] = 1
                results = catalog(
                    object_provides=IPart.__identifier__,
                    path='/',
                    moduleId=partModuleId)
                for brain in results:
                    obj = brain.getObject()
                    project.parts.append(obj)
            else:
                partModuleIds[partModuleId ] += 1
    
    # TODO percentage of routed connections in each view
    
    # TODO handle custom parts (as children of this project)
    
    # Parse code files
    project.code = ""
    for codeFileName in codeFileNames:
        code = zf.read(codeFileName)
        project.code += codeFileName
        project.code += '\n'
        project.code += code
        project.code += '\n'
    #TODO rather add as children of this project?