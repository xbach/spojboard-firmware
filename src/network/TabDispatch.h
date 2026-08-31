#ifndef TAB_DISPATCH_H
#define TAB_DISPATCH_H

// Which per-tab config parsers a POST is allowed to run. Pure: no Arduino, no
// server headers, tested in test/test_tabdispatch.
//
// The config form does PER-TAB SAVE: the page posts a `tab` field naming the
// panel it submitted, and handleSave() runs only that panel's parser. Those
// parsers contain assignments of the form
//
//     config->showPlatform = server->hasArg("show_platform");
//
// which read an absent checkbox as UNCHECKED. That is correct for the tab that
// was actually submitted and catastrophic for one that was not.
//
// The trap is `tab == "all"`. It means "the whole form was sent", and the whole
// form is every tab ONLY when every tab was rendered. AP mode renders the
// connection and hardware tabs and nothing else (DashboardPage.cpp), and its save
// posts no `tab` at all -- so it arrives as "all" and used to run every parser
// against a form containing none of their fields, silently clearing
// showPlatform, scrollEnabled, showMultipleTimes, debugMode and weatherEnabled
// on every setup save.
//
// This function deliberately MIRRORS DashboardPage's rendering condition. A
// tab's persistence rule and its rendering rule have to move in the same edit;
// keeping both expressed against apModeActive is what makes that possible,
// instead of each side inferring what the other did.
//
// @param postedTab  the POST's `tab` value, or "all" when it sent none
// @param tabName    the tab whose parser is about to run
// @param apModeActive whether the page that produced this POST was the AP one
bool tabSubmitted(const char* postedTab, const char* tabName, bool apModeActive);

#endif // TAB_DISPATCH_H
