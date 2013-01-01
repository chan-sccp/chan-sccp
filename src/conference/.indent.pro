-linux 	/* Use Linux Code Style */
-l500 	/* Max Line Length */
-c113 	/* Comments start at column 113 */
-hnl 	/* Honour new lines */
-nbbo 	/* Break after Boolean operator, if line needs to be broken -> boolean operator always at the end */
-ppi4	/* Preprocessor indentation -> 4 */
-sob 	/* swallow optional blank lines */
-cd81	/* when function declarations are comment, put them in column 81 */
-bad	/* force newline after each declaration block */
-bap 	/* force newline after a procedure */
-npsl	/* put the type of a procedure on the same line as its name */
-lp	/* on lines with unclosed left parenthesis, continuation lines will be lined up to start at the character position just after the left parenthesis */
-cli8	/* Case indentation */
-lp	/* Continue at Parentheses */
-ut	/* Use Tabs */
-ts8	/* Tab Size 8 */
-bap	/* Blank line after procedure */
-cs	/* Space after cast */
-il0	/* Indent Label */
-nss	/* Dont Space Special Semicolon */
-ppi0	/* Preprocessor Indentation */

/* Declarations / Identifiers */
//-di32	/* Line up identifiers in column 32 */
-nbc	/* No Blank Line after comma's */
-bad	/* Black Line after Declarations */
-cd113	/* Comment column for Declarations */

/* type definitions */
-T SCCP_LIST_HEAD 
-T SCCP_RWLIST_HEAD
-T SCCP_LIST_ENTRY
-T SCCP_RWLIST_ENTRY
-T sccp_push_result_t
-T boolean_t

/* comments */
-lc500 	/* Max comment line length */
-cp113 	/* Comment after #else and #endif to column 113 */
-bbb	/* force blank lines before boxed comment blocks */
-fc1	/* Format first column comments */

