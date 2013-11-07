Thin_Vessel_Segmentation
========================
This is my master project about segmentation on 3D thin vessels. The project was build with Visual Studio 2010 with 64-bit Windows 7. 

We know that OpenCV does not support 3D that well. The projects contains a lot of data structures as well as image processing function for 3D datas. If you are working with 3D data and want something as a head start, this should be a very good choice. 

Some functions of the project:

 - Template class Data3D<T> for 3D Data
   - Reading/Writing Raw Data
 - Visualization
   - Slice by Slice
   - Maximum Intensity Projection
 - Image Processing
   - GaussianBlur 3D
   - Convolution 3D
   - Histogram
   - Thresholding
   - Normalization
 - Rings Deduction of 3D Data
 - Compute Vesselness Based on Hessian Matrix

and a lot more ...

Dependencies
========================
In order to run the project, you will have the install the following softwares, 

 - OpenCV
 - Matlab
 - OpenGL
  
Some important dlls have been inlcuded in the project under VesselNess/depencencies. Run the SET_PATH.bat to add the dlls to your Environment PATH. After that, you will be able run most parts of the project other than anything related with MIP (Maximum Intensity Projection). 

Data
========================
The data we have been working with are 3D volumns and they are hugh. I don't want to commit them to github. Feel free to email me for the data if you want (yzhong.cs@gmail.com). 
