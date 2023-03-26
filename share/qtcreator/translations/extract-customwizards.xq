let $prefix := string("QT_TRANSLATE_NOOP(&quot;QtC::ProjectExplorer&quot;, &quot;")
let $suffix := concat("&quot;)", codepoints-to-string(10))
for $file in tokenize($files, string("\|"))
    let $doc := doc($file)
    for $text in ($doc/*:wizard/*:description, $doc/*:wizard/*:displayname, $doc/*:wizard/*:displaycategory, $doc/*:wizard/*:fieldpagetitle, $doc/*:wizard/*:fields/*:field/*:fielddescription, $doc/*:wizard/*:fields/*:field/*:fieldcontrol/*:comboentries/*:comboentry/*:comboentrytext, $doc/*:wizard/*:validationrules/*:validationrule/*:message)
        return fn:concat($prefix, data($text), $suffix)
