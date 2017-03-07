/* File: Ratings.h
 * Interface to libRatings.A.dylib 1.0.
 *************************************/
 
/* Adds 'rating' to the set.
 *      rating: Each character adds 1 to the numeric rating
 *              Example: "" = 0, "*" = 1, "**" = 2, "wer " = 4.
 */
void addRating(char *rating);
 
/* Returns the number of ratings in the set.
 */
int ratings(void);
 
/* Returns the mean rating of the set.
 */
char *meanRating(void);
 
/* Clears the set.
 */
void clearRatings(void);

