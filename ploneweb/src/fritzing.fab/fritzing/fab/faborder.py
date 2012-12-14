import zipfile
from cStringIO import StringIO

from five import grok
from z3c.form import field, button
from zope.lifecycleevent.interfaces import IObjectAddedEvent, IObjectModifiedEvent, IObjectMovedEvent
from zope.component import createObject

from plone.directives import dexterity, form
from plone.dexterity.content import Item

from Products.statusmessages.interfaces import IStatusMessage
from Products.CMFCore.utils import getToolByName
from Products.CMFCore.interfaces import IActionSucceededEvent

from fritzing.fab.interfaces import IFabOrder, ISketch
from fritzing.fab import getboardsize
from fritzing.fab.tools import getStateId, sendStatusMail, sendSketchUpdateMail, recalculatePrices
from fritzing.fab import _


class Index(grok.View):
    """Review the order
    """
    grok.require('zope2.View')
    grok.context(IFabOrder)
    
    label = _(u"Order your PCBs")
    description = _(u"Review the order")
    
    # make tools availible to the template as view.toolname()
    from fritzing.fab.tools \
        import encodeFilename, getStateId, getStateTitle, isStateId
    
    def update(self):
        member = self.context.portal_membership.getAuthenticatedMember()
        self.isManager = member.has_role('Manager')
        if not (self.isManager):
            self.request.set('disable_border', 1)
        
        if self.context.shipTo:
            self.shipToTitle = IFabOrder['shipTo'].vocabulary.getTerm(self.context.shipTo).title

        if self.context.shippingCountry:
            self.shippingCountryTitle = IFabOrder['shippingCountry'].vocabulary.getTerm(self.context.shippingCountry).title
        
        primeCostsPerSquareCm = 0.21
        earningsPerSquareCm = 0.7 * (self.context.pricePerSquareCm - primeCostsPerSquareCm)
        devHourCosts = 50.0
        self.devMinutes = (earningsPerSquareCm * self.context.area) / (devHourCosts / 60.0)


class Edit(dexterity.EditForm):
    """Edit the order
    """
    grok.name('edit')
    grok.require('cmf.ModifyPortalContent')
    grok.context(IFabOrder)
    
    schema = IFabOrder
    
    label = _(u"Edit order")
    description = _(u"Edit the order")


class Checkout(grok.View):
    """Order checkout
    """
    grok.name('checkout')
    grok.require('cmf.ModifyPortalContent')
    grok.context(IFabOrder)
    
    label = _(u"Checkout")
    description = _(u"Order checkout")
    
    def render(self):
        faborderURL = self.context.absolute_url()
        self.request.response.redirect(faborderURL)


class CheckoutFail(grok.View):
    """Order checkout fail
    """
    grok.name('checkout_fail')
    grok.require('zope2.View')
    grok.context(IFabOrder)
    
    label = _(u"Checkout")
    description = _(u"Order checkout")
    
    def render(self):       
        faborderURL = self.context.absolute_url()
        IStatusMessage(self.request).addStatusMessage(
            _(u"Your payment was not completed. If you cannot solve the problem, please don't hesitate to contact us."), "warning")
        self.request.response.redirect(faborderURL)


class CheckoutSuccess(grok.View):
    """Order checkout success
    """
    grok.name('checkout_success')
    grok.require('zope2.View')
    grok.context(IFabOrder)
    
    label = _(u"Checkout")
    description = _(u"Order checkout")
    
    def render(self):
        faborderURL = self.context.absolute_url()
        IStatusMessage(self.request).addStatusMessage(
            _(u"Your payment was successful!"), "info")
        self.request.response.redirect(faborderURL)


class PayPalCheckout(grok.View):
    """Order checkout
    """
    grok.name('paypal_checkout')
    grok.require('zope2.View')
    grok.context(IFabOrder)
    
    label = _(u"PayPal Checkout")
    description = _(u"Order checkout")
    
    
    def update(self):
        # = getToolByName(self, 'portal_workflow')
        #review_state = getStateId(None, self.context, portal_workflow)
        
        #if review_state != 'open':
        #    self.addStatusMessage(_(u"Already checked out."), "info")
        #    return
        #if not self.context.area > 0:
        #    self.addStatusMessage(_(u"Sketches missing/invalid, checkout aborted."), "error")
        #    return
        
        #portal_workflow.doActionFor(self.context, action='submit')
        
        # send e-mails
        #sendStatusMail(self.context)
        
        self.request.set('disable_border', 1)
    
    def addStatusMessage(self, message, messageType):
        IStatusMessage(self.request).addStatusMessage(message, messageType)
        faborderURL = self.context.absolute_url()
        self.request.response.redirect(faborderURL)


class StatusMail(grok.View):
    """This is for debugging
    """
    grok.name('statusmail')
    grok.require('zope2.View')
    grok.context(IFabOrder)
    
    def render(self):
        self.response.setHeader("Content-Type", "text/plain")
        return sendStatusMail(self.context, justReturn=True)


@grok.subscribe(IFabOrder, IActionSucceededEvent)
def workflowTransitionHandler(faborder, event):
    """event-handler for workflow transitions on IFabOrder instances
    """
    if event.action in ('submit', 'complete'):
        sendStatusMail(faborder)


@grok.subscribe(IFabOrder, IObjectAddedEvent)
def orderAddedHandler(faborder, event):
    faborders = faborder.aq_parent
    faborder.productionRound = faborders.currentProductionRound


@grok.subscribe(IFabOrder, IObjectModifiedEvent)
def orderModifiedHandler(faborder, event):
    # see http://www.postsitter.de/briefversand/europa.htm
    if faborder.shippingCountry in ['DE']:
        faborder.shipTo = 'germany'
    elif faborder.shippingCountry in [
            'AL', 'AD', 'AM', 'AZ', 'BY', 'BE', 'BA', 'BG', 'HR', 'DK', 'EE', 
            'FO', 'FI', 'FR', 'GF', 'GE', 'GR', 'GL', 'UK', 'GP', 'IE', 'IS', 
            'IT', 'KZ', 'LT', 'LI', 'LU', 'MK', 'MT', 'MQ', 'YT', 'MD', 'MC', 
            'NL', 'NO', 'AT', 'PL', 'PT', 'RE', 'RO', 'RU', 'SM', 'SE', 'SI', 
            'SK', 'CH', 'ES', 'PM', 'CZ', 'TR', 'UA', 'HU', 'VA', 'YU', 'CY']:
        faborder.shipTo = 'eu'
    else:
        faborder.shipTo = 'world'
    recalculatePrices(faborder)


@grok.subscribe(ISketch, IObjectModifiedEvent)
@grok.subscribe(ISketch, IObjectMovedEvent)
def sketchModifiedHandler(sketch, event):
    faborder = sketch.aq_parent
    
    # sum up the areas of all sketches and the number of quality checks
    faborder.area = 0
    faborder.numberOfQualityChecks = 0
    for sketch in faborder.listFolderContents():
        faborder.area += sketch.copies * sketch.area
        if sketch.check:
            faborder.numberOfQualityChecks += 1
    
    # choose discount
    if (faborder.area < 50):
        faborder.pricePerSquareCm = 0.70
    elif (faborder.area < 100):
        faborder.pricePerSquareCm = 0.60
    elif (faborder.area < 200):
        faborder.pricePerSquareCm = 0.50
    elif (faborder.area < 500):
        faborder.pricePerSquareCm = 0.40
    else:
        faborder.pricePerSquareCm = 0.35
    
    recalculatePrices(faborder)


class AddForm(dexterity.AddForm):
    """adds a sketch
    """
    grok.name('sketch')
    grok.require('cmf.AddPortalContent')
    grok.context(IFabOrder)
    
    schema = ISketch
    
    label = _(u"Add Sketch")
    description = u''
    
    def create(self, data):
        sketch = createObject('sketch')
        sketch.id = data['orderItem'].filename.encode("ascii")
        
        # lets make sure this file doesn't already exist
        if self.context.hasObject(sketch.id):
            IStatusMessage(self.request).addStatusMessage(
                _(u"A Sketch with this name already exists."), "error")
            return None
        
        sketch.title = data['orderItem'].filename
        sketch.orderItem = data['orderItem']
        sketch.copies = data['copies']
        return sketch
    
    def add(self, object):
        if isinstance(object, Item):
            self.context._setObject(object.id, object)


class SketchUpdateForm(form.EditForm):
    """ updates the sketch file for an order
    """
    grok.name('update')
    grok.require('cmf.ModifyPortalContent')
    grok.context(ISketch)

    fields = field.Fields(ISketch).select(
        'orderItem',
    )

    label = _(u"Update sketch file")
    description = _(u"As long as the size of the board is not changing, you may replace your sketch file until the deadline.")

    @button.buttonAndHandler(_(u'Save'))
    def handleApply(self, action):
        data, errors = self.extractData()
        if errors:
            self.status = self.formErrorsMessage
            return

        newSketchFile = data['orderItem']
        fzzData = StringIO(newSketchFile.data)
        zf = None
        zf = zipfile.ZipFile(fzzData)
        pairs = getboardsize.fromZipFile(zf, newSketchFile.filename)
        boards = [
            {'width':pairs[i] / 10, 'height':pairs[i+1] / 10} 
            for i in range(0, len(pairs), 2)
        ]
        totalArea = 0
        for board in boards:
            totalArea += board['width'] * board['height']

        oldSketch = self.aq_parent
        if abs(totalArea - oldSketch.area) > 0.1:
            self.status = _(u"Sorry, you can only replace the sketch if the board size has not changed. "
                "If you want to proceed, please contact us to cancel the current order and submit a new one.")
            return
        else:
            # TODO: set status to "not checked"
            IStatusMessage(self.request).addStatusMessage(
                _(u"The sketch has been successfully updated."), "info")
            sendSketchUpdateMail(self.aq_parent)
            contextURL = self.context.absolute_url()
            self.request.response.redirect(contextURL)


    @button.buttonAndHandler(_(u'Cancel'))
    def handleCancel(self, action):
        contextURL = self.context.absolute_url()
        self.request.response.redirect(contextURL)
    

class ShippingEditForm(form.EditForm):
    """ edits the shipping info
    """
    grok.name('edit-shipping-info')
    grok.require('cmf.ModifyPortalContent')
    grok.context(IFabOrder)

    fields = field.Fields(IFabOrder).select(
        'shippingFirstName',
        'shippingLastName',
        'shippingCompany',
        'shippingStreet',
        'shippingAdditional',
        'shippingCity',
        'shippingZIP',
        'shippingCountry',
        'shippingExpress',
        'email'
    )

    label = _(u"Provide shipping info")
    description = u'Where should we send these boards to?'

