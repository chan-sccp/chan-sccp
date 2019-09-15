/* Main Style */
-linux 	// Use Linux Code Style */

/* Changes from Style */
-l225 	// Max Line Length
-hnl 	// Honour new lines
-nbbo 	// Break after Boolean operator, if line needs to be broken -> boolean operator always at the end
-ppi4	// Preprocessor indentation -> 4
-sob 	// swallow optional blank lines
-bad	// force newline after each declaration block
-bap 	// force newline after a procedure
-npsl	// put the type of a procedure on the same line as its name
-lp	// on lines with unclosed left parenthesis, continuation lines will be lined up to start at the character position just after the left parenthesis
-lp	// Continue at Parentheses
-ut	// Use Tabs
-ts8	// Tab Size 8
-bap	// Blank line after procedure
-cs	// Space after cast
-il0	// Indent Label
-cli8	// Case Label indentation
-nss	// Dont Space Special Semicolon
-ppi0	// Preprocessor Indentation

/* Declarations / Identifiers */
-nbc	// No Blank Line after comma's
-bad	// Black Line after Declarations
//-di32	// Line up identifiers in column 32

/* type definitions */
-T SCCP_LIST_HEAD 
-T SCCP_RWLIST_HEAD
-T SCCP_LIST_ENTRY
-T SCCP_RWLIST_ENTRY
-T SCCP_API
-T SCCP_CALL
-T sccp_push_result_t
-T boolean_t
-T sessionPtr
-T devicePtr
-T linePtr
-T channelPtr
-T lineDevicePtr
-T conferencePtr
-T constSessionPtr
-T constDevicePtr
-T constLinePtr
-T constChannelPtr
-T constLineDevicePtr
-T constConferencePtr
-T messagePtr
-T constMessagePtr

/* comments */
-bbb	// force blank lines before boxed comment blocks
-lc700 	// Max comment line length
-d0	// Set indentation of comments not to the right of code to n spaces
-c153 	// Comments start at column 153
-cd153	// Comment column for Function Declarations
-cp153 	// Comment after #else and #endif to column 153
--cuddle-do-while
--cuddle-else
-fc1	// Format first column comments
/*
-fca // Do not disable all formatting of comments.
-fnc // Fix nested comments
-sc	// Put the ‘*’ character at the left of comments
*/

/* new */
--swallow-optional-blank-lines
--dont-break-procedure-type
--left-justify-declarations
