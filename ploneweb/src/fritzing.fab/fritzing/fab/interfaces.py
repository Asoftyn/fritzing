from five import grok
from zope.schema import Text, TextLine, ASCIILine, Int, Float, Choice, Bool, Date, Datetime
from z3c.form import validator
from zope.schema.vocabulary import SimpleVocabulary, SimpleTerm

from plone.directives import form
from plone.namedfile.field import NamedBlobFile
from plone.app.textfield import RichText

from fritzing.fab.constraints import checkEMail, SketchFileValidator
from fritzing.fab import _


class ISketch(form.Schema):
    """A Fritzing Sketch file
    """
    
    orderItem = NamedBlobFile(
        title = _(u"Sketch file"),
        description = _(u"The .fzz file of your sketch"))
    
    copies = Int(
        title = _(u"Copies"),
        description = _(u"The more copies, the cheaper each one gets"),
        min = 1,
        default = 1)

    form.omitted('check')
    publishable = Bool(
        title=_(u"Publish in project gallery"),
        description=_(u"Make this project visible in the public gallery"),
        default = False,
        required = False)

    form.omitted('check')
    checked = Bool(
        title=_(u"Quality checked"),
        description=_(u"The checking status of this sketch"),
        default = False,
        required = False)
    
    form.omitted('check')
    check = Bool(
        title = _(u"File Check"),
        description = _(u"Have us check your design for an extra 4 EUR"),
        default = True)
    
    form.omitted(
        'width', 
        'height', 
        'area')
    
    width = Float(
        title = _(u"Width"),
        description = _(u"The width of this sketch in cm"),
        min = 0.0,
        default = 0.0)
    
    height = Float(
        title = _(u"Height"),
        description = _(u"The height of this sketch in cm"),
        min = 0.0,
        default = 0.0)
    
    area = Float(
        title = _(u"Area"),
        description = _(u"The area of this sketch in cm^2"),
        min = 0.0,
        default = 0.0)


validator.WidgetValidatorDiscriminators(SketchFileValidator, field = ISketch['orderItem'])
grok.global_adapter(SketchFileValidator)


class IFabOrder(form.Schema):
    """Fritzing Fab order details
    """
    
    shippingFirstName = TextLine(
        title = _(u"First Name"),
        description = u'')

    shippingLastName = TextLine(
        title = _(u"Last Name"),
        description = u'')

    shippingCompany = TextLine(
        title = _(u"Company"),
        description = _(u"optional"),
        required = False)

    shippingStreet = TextLine(
        title = _(u"Street and Number"),
        description = u'')

    shippingAdditional = TextLine(
        title = _(u"Additional info"),
        description = _(u"optional"),
        required = False)

    shippingCity = TextLine(
        title = _(u"City"),
        description = u'')

    shippingZIP = TextLine(
        title = _(u"ZIP"),
        description = u'')

    shippingCountry = Choice(
        title = _(u"Country"),
        description = u'',
        vocabulary = SimpleVocabulary([
            SimpleTerm(value=u'AF', title=_(u'Afghanistan')),
            SimpleTerm(value=u'AL', title=_(u'Albania')),
            SimpleTerm(value=u'DZ', title=_(u'Algeria')),
            SimpleTerm(value=u'AS', title=_(u'American Samoa')),
            SimpleTerm(value=u'AD', title=_(u'Andorra')),
            SimpleTerm(value=u'AO', title=_(u'Angola')),
            SimpleTerm(value=u'AI', title=_(u'Anguilla')),
            SimpleTerm(value=u'AQ', title=_(u'Antarctica')),
            SimpleTerm(value=u'AG', title=_(u'Antigua And Barbuda')),
            SimpleTerm(value=u'AR', title=_(u'Argentina')),
            SimpleTerm(value=u'AM', title=_(u'Armenia')),
            SimpleTerm(value=u'AW', title=_(u'Aruba')),
            SimpleTerm(value=u'AU', title=_(u'Australia')),
            SimpleTerm(value=u'AT', title=_(u'Austria')),
            SimpleTerm(value=u'AZ', title=_(u'Azerbaijan')),
            SimpleTerm(value=u'BS', title=_(u'Bahamas')),
            SimpleTerm(value=u'BH', title=_(u'Bahrain')),
            SimpleTerm(value=u'BD', title=_(u'Bangladesh')),
            SimpleTerm(value=u'BB', title=_(u'Barbados')),
            SimpleTerm(value=u'BY', title=_(u'Belarus')),
            SimpleTerm(value=u'BE', title=_(u'Belgium')),
            SimpleTerm(value=u'BZ', title=_(u'Belize')),
            SimpleTerm(value=u'BJ', title=_(u'Benin')),
            SimpleTerm(value=u'BM', title=_(u'Bermuda')),
            SimpleTerm(value=u'BT', title=_(u'Bhutan')),
            SimpleTerm(value=u'BO', title=_(u'Bolivia')),
            SimpleTerm(value=u'BA', title=_(u'Bosnia And Herzegowina')),
            SimpleTerm(value=u'BW', title=_(u'Botswana')),
            SimpleTerm(value=u'BV', title=_(u'Bouvet Island')),
            SimpleTerm(value=u'BR', title=_(u'Brazil')),
            SimpleTerm(value=u'BN', title=_(u'Brunei Darussalam')),
            SimpleTerm(value=u'BG', title=_(u'Bulgaria')),
            SimpleTerm(value=u'BF', title=_(u'Burkina Faso')),
            SimpleTerm(value=u'BI', title=_(u'Burundi')),
            SimpleTerm(value=u'KH', title=_(u'Cambodia')),
            SimpleTerm(value=u'CM', title=_(u'Cameroon')),
            SimpleTerm(value=u'CA', title=_(u'Canada')),
            SimpleTerm(value=u'CV', title=_(u'Cape Verde')),
            SimpleTerm(value=u'KY', title=_(u'Cayman Islands')),
            SimpleTerm(value=u'CF', title=_(u'Central African Rep')),
            SimpleTerm(value=u'TD', title=_(u'Chad')),
            SimpleTerm(value=u'CL', title=_(u'Chile')),
            SimpleTerm(value=u'CN', title=_(u'China')),
            SimpleTerm(value=u'CX', title=_(u'Christmas Island')),
            SimpleTerm(value=u'CC', title=_(u'Cocos Islands')),
            SimpleTerm(value=u'CO', title=_(u'Colombia')),
            SimpleTerm(value=u'KM', title=_(u'Comoros')),
            SimpleTerm(value=u'CG', title=_(u'Congo')),
            SimpleTerm(value=u'CK', title=_(u'Cook Islands')),
            SimpleTerm(value=u'CR', title=_(u'Costa Rica')),
            SimpleTerm(value=u'CI', title=_(u'Cote D`ivoire')),
            SimpleTerm(value=u'HR', title=_(u'Croatia')),
            SimpleTerm(value=u'CU', title=_(u'Cuba')),
            SimpleTerm(value=u'CY', title=_(u'Cyprus')),
            SimpleTerm(value=u'CZ', title=_(u'Czech Republic')),
            SimpleTerm(value=u'DK', title=_(u'Denmark')),
            SimpleTerm(value=u'DJ', title=_(u'Djibouti')),
            SimpleTerm(value=u'DM', title=_(u'Dominica')),
            SimpleTerm(value=u'DO', title=_(u'Dominican Republic')),
            SimpleTerm(value=u'TP', title=_(u'East Timor')),
            SimpleTerm(value=u'EC', title=_(u'Ecuador')),
            SimpleTerm(value=u'EG', title=_(u'Egypt')),
            SimpleTerm(value=u'SV', title=_(u'El Salvador')),
            SimpleTerm(value=u'GQ', title=_(u'Equatorial Guinea')),
            SimpleTerm(value=u'ER', title=_(u'Eritrea')),
            SimpleTerm(value=u'EE', title=_(u'Estonia')),
            SimpleTerm(value=u'ET', title=_(u'Ethiopia')),
            SimpleTerm(value=u'FK', title=_(u'Falkland Islands (Malvinas)')),
            SimpleTerm(value=u'FO', title=_(u'Faroe Islands')),
            SimpleTerm(value=u'FJ', title=_(u'Fiji')),
            SimpleTerm(value=u'FI', title=_(u'Finland')),
            SimpleTerm(value=u'FR', title=_(u'France')),
            SimpleTerm(value=u'GF', title=_(u'French Guiana')),
            SimpleTerm(value=u'PF', title=_(u'French Polynesia')),
            SimpleTerm(value=u'TF', title=_(u'French S. Territories')),
            SimpleTerm(value=u'GA', title=_(u'Gabon')),
            SimpleTerm(value=u'GM', title=_(u'Gambia')),
            SimpleTerm(value=u'GE', title=_(u'Georgia')),
            SimpleTerm(value=u'DE', title=_(u'Germany')),
            SimpleTerm(value=u'GH', title=_(u'Ghana')),
            SimpleTerm(value=u'GI', title=_(u'Gibraltar')),
            SimpleTerm(value=u'GR', title=_(u'Greece')),
            SimpleTerm(value=u'GL', title=_(u'Greenland')),
            SimpleTerm(value=u'GD', title=_(u'Grenada')),
            SimpleTerm(value=u'GP', title=_(u'Guadeloupe')),
            SimpleTerm(value=u'GU', title=_(u'Guam')),
            SimpleTerm(value=u'GT', title=_(u'Guatemala')),
            SimpleTerm(value=u'GN', title=_(u'Guinea')),
            SimpleTerm(value=u'GW', title=_(u'Guinea-bissau')),
            SimpleTerm(value=u'GY', title=_(u'Guyana')),
            SimpleTerm(value=u'HT', title=_(u'Haiti')),
            SimpleTerm(value=u'HN', title=_(u'Honduras')),
            SimpleTerm(value=u'HK', title=_(u'Hong Kong')),
            SimpleTerm(value=u'HU', title=_(u'Hungary')),
            SimpleTerm(value=u'IS', title=_(u'Iceland')),
            SimpleTerm(value=u'IN', title=_(u'India')),
            SimpleTerm(value=u'ID', title=_(u'Indonesia')),
            SimpleTerm(value=u'IR', title=_(u'Iran')),
            SimpleTerm(value=u'IQ', title=_(u'Iraq')),
            SimpleTerm(value=u'IE', title=_(u'Ireland')),
            SimpleTerm(value=u'IL', title=_(u'Israel')),
            SimpleTerm(value=u'IT', title=_(u'Italy')),
            SimpleTerm(value=u'JM', title=_(u'Jamaica')),
            SimpleTerm(value=u'JP', title=_(u'Japan')),
            SimpleTerm(value=u'JO', title=_(u'Jordan')),
            SimpleTerm(value=u'KZ', title=_(u'Kazakhstan')),
            SimpleTerm(value=u'KE', title=_(u'Kenya')),
            SimpleTerm(value=u'KI', title=_(u'Kiribati')),
            SimpleTerm(value=u'KP', title=_(u'Korea (North)')),
            SimpleTerm(value=u'KR', title=_(u'Korea (South)')),
            SimpleTerm(value=u'KW', title=_(u'Kuwait')),
            SimpleTerm(value=u'KG', title=_(u'Kyrgyzstan')),
            SimpleTerm(value=u'LA', title=_(u'Laos')),
            SimpleTerm(value=u'LV', title=_(u'Latvia')),
            SimpleTerm(value=u'LB', title=_(u'Lebanon')),
            SimpleTerm(value=u'LS', title=_(u'Lesotho')),
            SimpleTerm(value=u'LR', title=_(u'Liberia')),
            SimpleTerm(value=u'LY', title=_(u'Libya')),
            SimpleTerm(value=u'LI', title=_(u'Liechtenstein')),
            SimpleTerm(value=u'LT', title=_(u'Lithuania')),
            SimpleTerm(value=u'LU', title=_(u'Luxembourg')),
            SimpleTerm(value=u'MO', title=_(u'Macau')),
            SimpleTerm(value=u'MK', title=_(u'Macedonia')),
            SimpleTerm(value=u'MG', title=_(u'Madagascar')),
            SimpleTerm(value=u'MW', title=_(u'Malawi')),
            SimpleTerm(value=u'MY', title=_(u'Malaysia')),
            SimpleTerm(value=u'MV', title=_(u'Maldives')),
            SimpleTerm(value=u'ML', title=_(u'Mali')),
            SimpleTerm(value=u'MT', title=_(u'Malta')),
            SimpleTerm(value=u'MH', title=_(u'Marshall Islands')),
            SimpleTerm(value=u'MQ', title=_(u'Martinique')),
            SimpleTerm(value=u'MR', title=_(u'Mauritania')),
            SimpleTerm(value=u'MU', title=_(u'Mauritius')),
            SimpleTerm(value=u'YT', title=_(u'Mayotte')),
            SimpleTerm(value=u'MX', title=_(u'Mexico')),
            SimpleTerm(value=u'FM', title=_(u'Micronesia')),
            SimpleTerm(value=u'MD', title=_(u'Moldova')),
            SimpleTerm(value=u'MC', title=_(u'Monaco')),
            SimpleTerm(value=u'MN', title=_(u'Mongolia')),
            SimpleTerm(value=u'MS', title=_(u'Montserrat')),
            SimpleTerm(value=u'MA', title=_(u'Morocco')),
            SimpleTerm(value=u'MZ', title=_(u'Mozambique')),
            SimpleTerm(value=u'MM', title=_(u'Myanmar')),
            SimpleTerm(value=u'NA', title=_(u'Namibia')),
            SimpleTerm(value=u'NR', title=_(u'Nauru')),
            SimpleTerm(value=u'NP', title=_(u'Nepal')),
            SimpleTerm(value=u'NL', title=_(u'Netherlands')),
            SimpleTerm(value=u'AN', title=_(u'Netherlands Antilles')),
            SimpleTerm(value=u'NC', title=_(u'New Caledonia')),
            SimpleTerm(value=u'NZ', title=_(u'New Zealand')),
            SimpleTerm(value=u'NI', title=_(u'Nicaragua')),
            SimpleTerm(value=u'NE', title=_(u'Niger')),
            SimpleTerm(value=u'NG', title=_(u'Nigeria')),
            SimpleTerm(value=u'NU', title=_(u'Niue')),
            SimpleTerm(value=u'NF', title=_(u'Norfolk Island')),
            SimpleTerm(value=u'MP', title=_(u'Northern Mariana Islands')),
            SimpleTerm(value=u'NO', title=_(u'Norway')),
            SimpleTerm(value=u'OM', title=_(u'Oman')),
            SimpleTerm(value=u'PK', title=_(u'Pakistan')),
            SimpleTerm(value=u'PW', title=_(u'Palau')),
            SimpleTerm(value=u'PA', title=_(u'Panama')),
            SimpleTerm(value=u'PG', title=_(u'Papua New Guinea')),
            SimpleTerm(value=u'PY', title=_(u'Paraguay')),
            SimpleTerm(value=u'PE', title=_(u'Peru')),
            SimpleTerm(value=u'PH', title=_(u'Philippines')),
            SimpleTerm(value=u'PN', title=_(u'Pitcairn')),
            SimpleTerm(value=u'PL', title=_(u'Poland')),
            SimpleTerm(value=u'PT', title=_(u'Portugal')),
            SimpleTerm(value=u'PR', title=_(u'Puerto Rico')),
            SimpleTerm(value=u'QA', title=_(u'Qatar')),
            SimpleTerm(value=u'RE', title=_(u'Reunion')),
            SimpleTerm(value=u'RO', title=_(u'Romania')),
            SimpleTerm(value=u'RU', title=_(u'Russian Federation')),
            SimpleTerm(value=u'RW', title=_(u'Rwanda')),
            SimpleTerm(value=u'KN', title=_(u'Saint Kitts And Nevis')),
            SimpleTerm(value=u'LC', title=_(u'Saint Lucia')),
            SimpleTerm(value=u'VC', title=_(u'St Vincent/Grenadines')),
            SimpleTerm(value=u'WS', title=_(u'Samoa')),
            SimpleTerm(value=u'SM', title=_(u'San Marino')),
            SimpleTerm(value=u'ST', title=_(u'Sao Tome')),
            SimpleTerm(value=u'SA', title=_(u'Saudi Arabia')),
            SimpleTerm(value=u'SN', title=_(u'Senegal')),
            SimpleTerm(value=u'SC', title=_(u'Seychelles')),
            SimpleTerm(value=u'SL', title=_(u'Sierra Leone')),
            SimpleTerm(value=u'SG', title=_(u'Singapore')),
            SimpleTerm(value=u'SK', title=_(u'Slovakia')),
            SimpleTerm(value=u'SI', title=_(u'Slovenia')),
            SimpleTerm(value=u'SB', title=_(u'Solomon Islands')),
            SimpleTerm(value=u'SO', title=_(u'Somalia')),
            SimpleTerm(value=u'ZA', title=_(u'South Africa')),
            SimpleTerm(value=u'ES', title=_(u'Spain')),
            SimpleTerm(value=u'LK', title=_(u'Sri Lanka')),
            SimpleTerm(value=u'SH', title=_(u'St. Helena')),
            SimpleTerm(value=u'PM', title=_(u'St.Pierre')),
            SimpleTerm(value=u'SD', title=_(u'Sudan')),
            SimpleTerm(value=u'SR', title=_(u'Suriname')),
            SimpleTerm(value=u'SZ', title=_(u'Swaziland')),
            SimpleTerm(value=u'SE', title=_(u'Sweden')),
            SimpleTerm(value=u'CH', title=_(u'Switzerland')),
            SimpleTerm(value=u'SY', title=_(u'Syrian Arab Republic')),
            SimpleTerm(value=u'TW', title=_(u'Taiwan')),
            SimpleTerm(value=u'TJ', title=_(u'Tajikistan')),
            SimpleTerm(value=u'TZ', title=_(u'Tanzania')),
            SimpleTerm(value=u'TH', title=_(u'Thailand')),
            SimpleTerm(value=u'TG', title=_(u'Togo')),
            SimpleTerm(value=u'TK', title=_(u'Tokelau')),
            SimpleTerm(value=u'TO', title=_(u'Tonga')),
            SimpleTerm(value=u'TT', title=_(u'Trinidad And Tobago')),
            SimpleTerm(value=u'TN', title=_(u'Tunisia')),
            SimpleTerm(value=u'TR', title=_(u'Turkey')),
            SimpleTerm(value=u'TM', title=_(u'Turkmenistan')),
            SimpleTerm(value=u'TV', title=_(u'Tuvalu')),
            SimpleTerm(value=u'UG', title=_(u'Uganda')),
            SimpleTerm(value=u'UA', title=_(u'Ukraine')),
            SimpleTerm(value=u'AE', title=_(u'United Arab Emirates')),
            SimpleTerm(value=u'UK', title=_(u'United Kingdom')),
            SimpleTerm(value=u'US', title=_(u'United States')),
            SimpleTerm(value=u'UY', title=_(u'Uruguay')),
            SimpleTerm(value=u'UZ', title=_(u'Uzbekistan')),
            SimpleTerm(value=u'VU', title=_(u'Vanuatu')),
            SimpleTerm(value=u'VA', title=_(u'Vatican City State')),
            SimpleTerm(value=u'VE', title=_(u'Venezuela')),
            SimpleTerm(value=u'VN', title=_(u'Viet Nam')),
            SimpleTerm(value=u'VG', title=_(u'Virgin Islands (British)')),
            SimpleTerm(value=u'VI', title=_(u'Virgin Islands (U.S.)')),
            SimpleTerm(value=u'EH', title=_(u'Western Sahara')),
            SimpleTerm(value=u'YE', title=_(u'Yemen')),
            SimpleTerm(value=u'YU', title=_(u'Yugoslavia')),
            SimpleTerm(value=u'ZR', title=_(u'Zaire')),
            SimpleTerm(value=u'ZM', title=_(u'Zambia')),
            SimpleTerm(value=u'ZW', title=_(u'Zimbabwe')),
        ]),
        default = 'DE')

    shippingExpress = Bool(
        title=_(u"Express Shipping"),
        description=_(u"Ships with super-fast DHL express (more info under ''Pricing')"),
        default = False,
        required = False)

    email = TextLine(
        title = _(u"E-Mail"),
        description = u'',
        constraint = checkEMail)

    shipTo = Choice(
        title = _(u"Shipping Area"),
        description = u'',
        vocabulary = SimpleVocabulary([
            SimpleTerm(value = u'germany', title = _(u'Germany')),
            SimpleTerm(value = u'eu', title = _(u'European Union (EU)')),
            SimpleTerm(value = u'world', title = _(u'Worldwide'))
        ]))
    
    form.omitted(
        'area', 
        'pricePerSquareCm', 
        'priceNetto', 
        'priceShipping', 
        'numberOfQualityChecks', 
        'priceQualityChecksNetto', 
        'taxesPercent', 
        'taxes', 
        'priceTotalNetto', 
        'priceTotalBrutto')
    
    telephone = ASCIILine(
        title = _(u"Telephone number"),
        description = u'',
        required = False)
    
    area = Float(
        title = _(u"Area"),
        description = _(u"The total area of all sketches in cm^2"),
        min = 0.0,
        default = 0.0)
    
    pricePerSquareCm = Float(
        title = _(u"Price per cm^2"),
        description = _(u"The price per cm^2 in Euro"),
        min = 0.0,
        default = 0.0)
    
    priceNetto = Float(
        title = _(u"Netto price"),
        description = _(u"The netto price without shipping in Euro"),
        min = 0.0,
        default = 0.0)
    
    priceShipping = Float(
        title = _(u"Shipping costs"),
        description = _(u"The shipping costs in Euro"),
        min = 0.0,
        default = 0.0)
    
    numberOfQualityChecks = Int(
        title = _(u"Number of quality checks"),
        description = _(u"Number of quality checks"),
        min = 0,
        default = 0)
    
    priceQualityChecksNetto = Float(
        title = _(u"Costs for quality checks"),
        description = _(u"The costs for quality checks in Euro"),
        min = 0.0,
        default = 0.0)
    
    taxesPercent = Float(
        title = _(u"Percent Taxes"),
        description = _(u"Taxes like VAT in Percent"),
        min = 0.0,
        default = 0.0)
    
    taxes = Float(
        title = _(u"Taxes"),
        description = _(u"Taxes like VAT"),
        min = 0.0,
        default = 0.0)
    
    priceTotalNetto = Float(
        title = _(u"Total Netto"),
        description = _(u"The netto price costs in Euro"),
        min = 0.0,
        default = 0.0)
    
    priceTotalBrutto = Float(
        title = _(u"Total"),
        description = _(u"The price including shipping costs and taxes in Euro"),
        min = 0.0,
        default = 0.0)
    
    trackingNumber = TextLine(
        title = _(u"Tracking Number"),
        description = _(u"The tracking number assigned by the parcel service"),
        required = False)

    productionRound = Int(
        title=_(u"Production Round #"),
        description=_(u"The production round that this order belongs to"),
        required = False)

    paymentType = Choice(
        title = _(u"Payment Type"),
        description = _(u"How would you like to pay?"),
        vocabulary = SimpleVocabulary([
            SimpleTerm(value = u'paypal', title = _(u'PayPal')),
            SimpleTerm(value = u'transfer', title = _(u'Bank Transfer')),
        ]))

    paymentId = TextLine(
        title = _(u"Payment ID"),
        description = _(u"The PayPal transaction number or other accounting number associated with this order"),
        required = False)

    paymentStatusMsg = TextLine(
        title = _(u"Payment Status Message"),
        description = _(u"Additional information about the payment status"),
        required = False)


class IFabOrders(form.Schema):
    """Fritzing Fab orders Folder
    """
    
    title = TextLine(
        title = _(u"Order-folder name"),
        description = _(u"The title of this fab-instance"))
    
    description = Text(
        title = _(u"Order-folder description"),
        description = _(u"The description (also subtitle) of this fab-instance"))

    form.fieldset(
        'productionRound', 
        label=_(u"Current Production Round"),
        fields=['currentProductionRound', 'currentProductionOpeningDate',
            'nextProductionClosingDate', 'nextProductionDelivery'])

    currentProductionRound = Int(
        title = _(u"Current production round ID"),
        description = _(u"Identification number of the current production round"),
        required = False)
    
    currentProductionOpeningDate = Datetime(
        title = _(u"Current production opening date"),
        description = _(u"Orders sent in after this date will be listed as current orders"),
        required = False)

    nextProductionClosingDate = Datetime(
        title = _(u"Current production closing date"),
        description = _(u"Orders must be sent in before this date to be included in the next production run"),
        required = False)

    nextProductionDelivery = Date(
        title = _(u"Current production delivery date"),
        description = _(u"Estimated delivery date of PCBs from the next production"),
        required = False)

    form.fieldset(
        'fees', 
        label=_(u"Fees"),
        fields=['shippingGermany', 'shippingGermanyExpress',
            'shippingEU', 'shippingEuExpress', 'shippingWorld', 'shippingWorldExpress',
            'taxesGermany', 'taxesEU', 'taxesWorld'])

    shippingGermany = Float(
        title = _(u"Shipping Costs Germany"),
        description = _(u"The shipping costs for Germany in Euro"),
        min = 0.0,
        default = 5.3)

    shippingGermanyExpress = Float(
        title = _(u"Shipping Costs Germany (Express)"),
        description = _(u"The shipping costs for Germany (Express) in Euro"),
        min = 0.0,
        default = 12.0)
    
    shippingEU = Float(
        title = _(u"Shipping Costs EU"),
        description = _(u"The shipping costs for the EU in Euro"),
        min = 0.0,
        default = 6.0)

    shippingEuExpress = Float(
        title = _(u"Shipping Costs EU (Express)"),
        description = _(u"The shipping costs for EU (Express) in Euro"),
        min = 0.0,
        default = 42.0)
    
    shippingWorld = Float(
        title = _(u"Shipping Costs outside EU"),
        description = _(u"The shipping costs for otside of the EU in Euro"),
        min = 0.0,
        default = 6.0)

    shippingWorldExpress = Float(
        title = _(u"Shipping Costs World (Express)"),
        description = _(u"The shipping costs for World (Express) in Euro"),
        min = 0.0,
        default = 62.0)
    
    taxesGermany = Float(
        title = _(u"Taxes Germany"),
        description = _(u"The taxes for Germany in Percent"),
        min = 0.0,
        default = 19.0)
    
    taxesEU = Float(
        title = _(u"Taxes EU"),
        description = _(u"The taxes for the EU in Percent"),
        min = 0.0,
        default = 19.0)
    
    taxesWorld = Float(
        title = _(u"Taxes outside EU"),
        description = _(u"The taxes for outside of the EU in Percent"),
        min = 0.0,
        default = 0.0)

    form.fieldset(
        'general', 
        label=_(u"General Settings"),
        fields=['salesEmail', 'paypalEmail'])

    salesEmail = TextLine(
        title = _(u"Sales e-mail"),
        description = _(u"Order status changes are e-mailed to this address"),
        default = _(u"fab@fritzing.org"),
        constraint = checkEMail)

    paypalEmail = TextLine(
        title = _(u"Paypal e-mail"),
        description = _(u"The mail address use by the PayPal seller account"),
        constraint = checkEMail)
    
    editableContent = RichText(
        title = _(u"Order-folder text"),
        description = _(u"The text of this fab-instance"))

