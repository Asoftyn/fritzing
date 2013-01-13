import zipfile
from cStringIO import StringIO
import DateTime

from Acquisition import aq_inner, aq_parent
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
        portal_workflow = getToolByName(self, 'portal_workflow')
        review_state = portal_workflow.getInfoFor(self.context, 'review_state')
        if review_state == 'in_process':
            IStatusMessage(self.request).addStatusMessage(
                _(u"Your payment was successful!"), "info")
        elif review_state == 'open':
            IStatusMessage(self.request).addStatusMessage(
                _(u"Your payment was not completed. If you cannot solve the problem, please don't hesitate to contact us."), "warning")
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
        self.request.set('disable_border', 1)


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
    faborders = faborder.__parent__

    if event.action in ('submit'):
        faborder.setEffectiveDate(DateTime.DateTime())
        faborder.productionRound = faborders.currentProductionRound
        faborder.reindexObject(idxs=['productionRound'])
        sendStatusMail(faborder)

    if event.action in ('produce'):
        faborder.shippingDate = faborders.nextProductionDelivery
        sendStatusMail(faborder)

    if event.action in ('complete'):
        sendStatusMail(faborder)



@grok.subscribe(IFabOrder, IObjectModifiedEvent)
def orderModifiedHandler(faborder, event):
    # see http://en.wikipedia.org/wiki/European_Union_value-added_tax_area
    # TODO: might need separate lists for EU tax zone and EU shipping cost zone
    if faborder.shippingCountry in ['DE']:
        faborder.shipTo = 'germany'
    elif faborder.shippingCountry in [
            'AT', 'AD', 'BE', 'BA', 'BG', 'CY', 'CZ', 'HR', 'DK', 'EE', 
            'FI', 'FR', 'GR', 'HU', 'IE', 'IT', 'LV', 'LT', 'LU', 'MC', 
            'MT', 'NL', 'PL', 'PT', 'RO', 'SK', 'SI', 'ES', 'SE', 'UK', ]:
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
    grok.require('zope2.View')
    grok.context(ISketch)

    fields = field.Fields(ISketch).select(
        'orderItem',
    )

    label = _(u"Update sketch file")
    description = _(u"As long as the size of the board is not changing, you may replace your sketch file until the deadline.")

    @button.buttonAndHandler(_(u'Save'))
    def handleApply(self, action):
        sketch = aq_inner(self.context)
        faborder = sketch.__parent__

        data, errors = self.extractData()
        if errors:
            self.status = self.formErrorsMessage
            return

        newSketchFile = data['orderItem']
        fzzData = StringIO(newSketchFile.data)
        zf = None
        zf = zipfile.ZipFile(fzzData)
        pairs = getboardsize.fromZipFile(zf, newSketchFile.filename)
        boards = [{'width':pairs[i] / 10, 'height':pairs[i+1] / 10} 
            for i in range(0, len(pairs), 2)]
        totalArea = 0
        for board in boards:
            totalArea += board['width'] * board['height']

        if abs(totalArea - sketch.area) > 0.1:
            self.status = _(u"Sorry, you can only replace the sketch if the board size has not changed. "
                "If you want to proceed, please contact us to cancel the current order and submit a new one.")
            return
        else:
            faborder.manage_permission('Add portal content', roles=['Manager', 'Owner'], acquire=True)
            sketch.manage_permission('Modify portal content', roles=['Manager', 'Owner'], acquire=True)
            sketch.manage_permission('Copy or Move', roles=['Manager', 'Owner'], acquire=True)

            newSketchId = newSketchFile.filename.encode("ascii")
            if newSketchId <> sketch.getId():
                faborder.manage_renameObject(sketch.getId(), newSketchId)
                sketch.title = newSketchFile.filename
            sketch.orderItem = newSketchFile
            sketch.checked = False
            sketch.reindexObject()

            faborder.manage_permission('Add portal content', roles=['Manager'], acquire=True)
            sketch.manage_permission('Modify portal content', roles=['Manager'], acquire=True)
            sketch.manage_permission('Copy or Move', roles=['Manager'], acquire=True)

            sendSketchUpdateMail(sketch)
            IStatusMessage(self.request).addStatusMessage(
                _(u"The sketch has been successfully updated."), "info")
            self.request.response.redirect(faborder.absolute_url())


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
        'telephone',
        'email'
    )

    label = _(u"Provide shipping info")
    description = u'Where should we send these boards to?'

