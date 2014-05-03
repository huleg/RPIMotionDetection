#include <stdio.h>
#include <opencv2/opencv.hpp>

#include "threshtree.h"
#include "depthtree.h"

using namespace cv;

/* Let only nodes with depth level = X passes. */
static int my_filter(Node *n){
	Blob* data = (Blob*) n->data;
	if( data->depth_level > 3 ) return 3;
	if( data->depth_level == 3 ) return 0;
	return 1;
}

static const char* window_name = "Blob Map";
static const char* window_options = window_name; //"Options";
static std::string image_filename;

static Mat input_image;
static bool redraw_pending = false;
static bool display_areas = true;
static bool display_filtered_areas = true;
static bool display_bounding_boxes = true;
static int algorithm = 1;
static int of_area_min = 5*4;
static int of_area_max = 1000*4;
static int of_tree_depth_min = 1;
static int of_tree_depth_max = 100;
static int of_area_depth_min = 0;//2;
static int of_area_depth_max = 255;//10;
static bool of_only_leafs = false;
static int output_scalefactor = 1;


// Thresh 
static int thresh = 60;
// Depth map (for algorithm 1)
unsigned char depth_map[256];

static ThreshtreeWorkspace *tworkspace = NULL;
static DepthtreeWorkspace *dworkspace = NULL;
static Blobtree *blob;


int detection_loop(std::string filename ){

	input_image = imread( filename, CV_LOAD_IMAGE_GRAYSCALE );
	if ( !input_image.data )
	{
		printf("No image data \n");
		return -1;
	}

	image_filename = filename;

	// Setup depth level map
	/* pixel value:   0 1 2 3 4 5 … thresh …                   … 255
	 *   depth map:   0 0 0 0 0 0 …    1     1 … 1 2 … 2 3 … 3 …
	 */
	int div=30;
	for( int i=0; i<256; i++){
		depth_map[i] = (i<thresh?0:(i-thresh)/30+1);
	}

	//====================================================
	const int W = input_image.size().width;
	const int H = input_image.size().height;
	const uchar* ptr = input_image.data;


	/* The workspaces depends on W and H
	 * Destroy old instances and reallocate memory
	 * This should be avoided in real applications.
	 * */
	threshtree_destroy_workspace( &tworkspace );
	depthtree_destroy_workspace( &dworkspace );
	blobtree_destroy(&blob);

	//Init workspaces
	threshtree_create_workspace( W, H, &tworkspace );
	if( tworkspace == NULL ){
		printf("Unable to create workspace.\n");
		return -1;
	}
	depthtree_create_workspace( W, H, &dworkspace );
	if( dworkspace == NULL ){
		printf("Unable to create workspace.\n");
		return -1;
	}

	blob = blobtree_create();


	// Set distance between compared pixels.	
	// Look at blobtree.h for more information.
	blobtree_set_grid(blob, 1,1);

	BlobtreeRect roi0 = {0,0,W, H };//shrink height because lowest rows contains noise.

	if( algorithm == 0 ){
		threshtree_find_blobs(blob, ptr, W, H, roi0, thresh, tworkspace);
	}else{
		depthtree_find_blobs(blob, ptr, W, H, roi0, depth_map, dworkspace);
	}

	/* Textual output of whole tree of blobs. */
	print_tree(blob->tree->root,0);

	return 0;
}

static void redraw(){

	if( blob == NULL ) return;

	redraw_pending = true;

	Mat color(input_image.size(),CV_8UC3);
	cv::cvtColor(input_image, color, CV_GRAY2BGR);
	if( !color.data ) return;

	/* Set output filters */
	//filter out small blobs
	blobtree_set_filter(blob, F_AREA_MIN,
			(of_area_min * of_area_min)/16 );

	//filter out big blobs
	blobtree_set_filter(blob, F_AREA_MAX,
			(of_area_max * of_area_max)/16 );

	//filter out top level blob for whole image
	blobtree_set_filter(blob, F_TREE_DEPTH_MIN,
			of_tree_depth_min );

	//filter out blobs included into bigger blobs
	blobtree_set_filter(blob, F_TREE_DEPTH_MAX,
			of_tree_depth_max );

	/* Note: Changeing depth_map could be the more efficient approach to
	 * get a similar filtering effect.*/
	//filter out blobs with lower depth values.
	blobtree_set_filter(blob, F_AREA_DEPTH_MIN,
			of_area_depth_min );

	//filter out blobs with higher depth values.
	blobtree_set_filter(blob, F_AREA_DEPTH_MAX,
			of_area_depth_max );

	/* Add own filter over function pointer */
	//blobtree_set_extra_filter(blob, my_filter);

	/* Only show leafs with above filtering effects */
	blobtree_set_filter(blob, F_ONLY_LEAFS, of_only_leafs?1:0 );


	
	/* Create data for graphical output */

	//fill color image with id map of filtered list of blobs
	//1. Create mapping for filtered list
	if( display_areas ){

		int seed;
		int *ids, *riv, *bif, *cm;

		if( algorithm == 1 ){
			ids = dworkspace->ids;
			cm = dworkspace->comp_same;
			riv = dworkspace->real_ids_inv;

			if( display_filtered_areas ){
				depthtree_filter_blob_ids(blob,dworkspace);
			}
			bif = dworkspace->blob_id_filtered;//maps  'unfiltered id' on 'parent filtered id'

		}else{
			ids = tworkspace->ids;
			cm = tworkspace->comp_same;
			riv = tworkspace->real_ids_inv;

			if( display_filtered_areas ){
				threshtree_filter_blob_ids(blob,tworkspace);
			}
			bif = tworkspace->blob_id_filtered;//maps  'unfiltered id' on 'parent filtered id'
		}

		for( int y=0, H=input_image.size().height; y<H; ++y){
			for( int x=0, W=input_image.size().width ; x<W; ++x) {
				if( display_filtered_areas && bif ){
					seed = *(bif+ *ids);
				}else{
					seed = *ids;

					/* This transformation just adjust the set of if- and else-branch.
					 * to avoid color flickering */
					seed = *(riv + *(cm + seed)) + 1 ;
				}

				/* To reduce color flickering for thresh changes
				 * eval variable which depends on some geometric information.
				 * Use seed(=id) to get the accociated node of the tree structure. 
				 * Adding 1 compensate the dummy element at position 0.
				*/
				Blob *pixblob = (Blob*) (blob->tree->root + seed )->data;
				seed = pixblob->area + pixblob->roi.x + pixblob->roi.y;

				const cv::Vec3b col( (seed*5*5+100)%256, (seed*7*7+10)%256, (seed*29*29+1)%256 );
				color.at<Vec3b>(y, x) = col;
				++ids;
			}
		}

	}


	BlobtreeRect *roi;
	Node *curNode = blobtree_first(blob);

	if( display_bounding_boxes ){
		int num=0;
		while( curNode != NULL ){
			num++;
			Blob *data = (Blob*)curNode->data;
			roi = &data->roi;
			int x     = roi->x + roi->width/2;
			int y     = roi->y + roi->height/2;
			/*
				 printf("Blob with id %i: x=%i y=%i w=%i h=%i area=%i, depthlevel=%i\n",data->id,
				 roi->x, roi->y,
				 roi->width, roi->height,
				 data->area,
				 data->depth_level
				 );
				 */

			cv::Rect cvRect(
					max(roi->x-1,0),
					max(roi->y-1,0),
					roi->width+2,
					roi->height+2
					);
			cvRect.width = min(cvRect.width,
					color.size().width - cvRect.x );
			cvRect.height = min(cvRect.height,
					color.size().height - cvRect.y );

			//const cv::Scalar col2(255.0f,255.0f- (num*29)%256,(num*5)%256);
			if( display_areas ){
				cv::rectangle( color, cvRect, cv::Scalar(255,255, 255), 1);
			}else{
				cv::rectangle( color, cvRect, cv::Scalar(100, 100, 255), 1);
			}

			curNode = blobtree_next(blob);
		}
	}


	cv::Mat out;
	cv::resize(color, out, color.size()*output_scalefactor, 0, 0, INTER_NEAREST);

	imshow( window_name, out );

	redraw_pending = false;
}


// ---- BEGIN Some callbacks for opencv gui ----

static void CB_Filter(int, void*){
	if( output_scalefactor < 1 ) output_scalefactor = 1;
	redraw();
}

static void CB_Thresh(int, void*){
	//Change of thresh require rerun of main algo.
	detection_loop(image_filename);
	redraw();
}

static void CB_Button1(int state, void* pointer){
	bool* p = (bool*) pointer;
	if( p == &display_bounding_boxes ){ display_bounding_boxes = (state>0); }
	if( p == &display_areas ){ display_areas = (state>0); }
	if( p == &display_filtered_areas ){ display_filtered_areas = (state>0); }
	if( p == &of_only_leafs ){ of_only_leafs = (state>0); }

	redraw();
}

// ---- END Some callbacks for opencv gui ----




int main(int argc, char** argv )
{

	//==========================================================
	//Setup

	const int loopMax = 120;
	int loop = 0;


	//Input handling
	if ( argc < 2 )
	{
		printf("usage: DisplayImage.out [Number of algorithm] [image path]\n"
				"Available Algorithms:\n"
				"0 - Thresh value split image in 'black' and 'white' areas. The areas\n"
				"    can be nested.\n"
				"1 - Depth map conquer pixels into different depth levels. Areas of\n"
				"    level X will be linked together if they are connected by areas\n"
				"    with levels > X. \n\n"
				);
	}else{
		algorithm = atoi(argv[1]);
		algorithm = (algorithm!=0?1:0);
	}

	if ( argc < 3){
		// Only use example images in "images/"
	}else{
		// Show argv[2] image and then example images
		loop = -1;
	}


	namedWindow(window_options, CV_WINDOW_AUTOSIZE );
	namedWindow(window_name, CV_WINDOW_AUTOSIZE );

	createTrackbar( "Thresh:", window_options, &thresh, 255, CB_Thresh );
	createTrackbar( "4*sqrt(Min Area):", window_options, &of_area_min, 200, CB_Filter );
	createTrackbar( "4*sqrt(Max Area):", window_options, &of_area_max, 200, CB_Filter );
	createTrackbar( "Min Tree Depth:", window_options, &of_tree_depth_min, 100, CB_Filter );
	createTrackbar( "Max Tree Depth:", window_options, &of_tree_depth_max, 100, CB_Filter );
	createTrackbar( "Min Area Level:", window_options, &of_area_depth_min, 255, CB_Filter );
	createTrackbar( "Max Area Level:", window_options, &of_area_depth_max, 255, CB_Filter );
	//createTrackbar( "Scale:", window_options, &output_scalefactor, 8, CB_Filter );

	createButton("Bounding boxes",CB_Button1,&display_bounding_boxes,CV_CHECKBOX, display_bounding_boxes );
	createButton("Only Leafs",CB_Button1,&of_only_leafs, CV_CHECKBOX, of_only_leafs );
	createButton("Coloured ids",CB_Button1,&display_areas,CV_CHECKBOX, display_areas );
	createButton("Only filtered coloured ids",CB_Button1,&display_filtered_areas,CV_CHECKBOX, display_filtered_areas );

	//Loop over list [Images] or [Image_Path, Images]
	while( loop < loopMax ){
		if( loop == -1 ){
			printf("Load image %s\n", argv[2]);
			std::string filename(argv[2]);
			detection_loop(filename);
		}else{
			std::ostringstream filename;
			filename << "images/" << loop << ".png" ;
			printf("Load image %s\n", filename.str().c_str());
			detection_loop(filename.str());
		}
		loop++;

		//====================================================================
		//Graphical Output

		redraw();			

		//Keyboard interaction
		int key = waitKey(0);
		printf("Key: %i\n", key);
		switch(key){
			case 27:{ //ESC-Key
								loop=loopMax+1;
								break;
							}
			case 65361:{ //Left
									 loop-=2;
									 if(loop<0 && argc<2 ) loop=0;
									 if(loop<-1)loop=0;
									 break;
								 }
		}

		//simple blocking
		while( redraw_pending ) {
			sleep(1);
		}

		//go back to first step
		if( loop==loopMax )
			loop = 0;

	}

	depthtree_destroy_workspace( &dworkspace );
	threshtree_destroy_workspace( &tworkspace );
	blobtree_destroy(&blob);


	return 0;
}