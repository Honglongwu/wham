/**************************************************************************************
 * File name: JenksBreaks.h
 *
 * Project: MapWindow Open Source (MapWinGis ActiveX control) 
 * Description: Declaration of JenksBreaks.h
 *
 **************************************************************************************
 * The contents of this file are subject to the Mozilla Public License Version 1.1
 * (the "License"); you may not use this file except in compliance with 
 * the License. You may obtain a copy of the License at http://www.mozilla.org/mpl/ 
 * See the License for the specific language governing rights and limitations
 * under the License.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ************************************************************************************** 
 * Contributor(s): 
 * (Open source contributors should list themselves and their modifications here). */
 // Sergei Leschinsky (lsu) 25 june 2010 - created the file
#pragma once
#include <vector>
#include <algorithm>
#include "float.h"

#ifndef MAX
#  define MIN(a,b)      ((a<b) ? a : b)
#  define MAX(a,b)      ((a>b) ? a : b)
#endif

class CJenksBreaks
{
	// data structures
	struct JenksData
	{
	public:	
		double value;
		double square;
		int classId;	// number of break to which a value belongs
	};

	struct JenksBreak
	{
	public:
		double value;
		double square;
		int count;
		double SDev;
		int startId;
		int endId;
		
		JenksBreak()
		{
			value = 0.0;
			square = 0.0;
			count = 0;
			SDev = 0.0;
			startId = -1;
			endId = -1;
		}
		
		// the formula for every class is: sum((a[k])^2) - sum(a[k])^2/n
		void RefreshStandardDeviations()
		{
			SDev = square - pow(value, 2.0)/(double)(endId - startId + 1);
		}
	};

public:

	// Constructor
	CJenksBreaks(std::vector<double>* values, int numClasses)
	{
		if (numClasses > 0)
		{
			
			_numClasses = numClasses;
			_numValues = values->size();
			
			double classCount = (double)_numValues/(double)numClasses;
			sort(values->begin(), values->end());		//values sould be sorted
			
			// fill values
			for (int i = 0; i < _numValues; i++)
			{
				JenksData data;
				data.value = (*values)[i];
				data.square = pow((*values)[i], 2.0);
				data.classId = int(floor(i/classCount));
				_values.push_back(data);
			}
			
			_classes.resize(_numClasses);

			// calculate initial deviations for classes
			int lastId = -1;
			for (int i = 0; i < _numValues; i++)
			{
				int classId = _values[i].classId;
				if (classId >= 0  && classId < _numClasses)
				{
					_classes[classId].value += _values[i].value;
					_classes[classId].square += _values[i].square;
					_classes[classId].count += 1;

					// saving bound between classes
					if (classId != lastId)
					{
						_classes[classId].startId = i;
						lastId = classId;
						
						if (classId > 0)
							_classes[classId - 1].endId = i - 1;
					}
				}
				else
				{
					// TODO: add error handling
				}
			}
			_classes[_numClasses - 1].endId = _numValues - 1;
			
			for (int i = 0; i < _numClasses; i++)
				_classes[i].RefreshStandardDeviations();
		}
	}
	~CJenksBreaks(){}
	
std::vector<int>* TestIt(std::vector<double>* data, int numClasses)
{
	int numValues = data->size();
	
	//std::vector<std::vector<int>> mat1;
	//std::vector<std::vector<float>> mat2;
	//mat1.resize(numValues + 1);
	//mat2.resize(numValues + 1);

	//for (int i = 0; i <= numValues; i++)
	//{
	//	mat1[i].resize(numClasses + 1);
	//	mat2[i].resize(numClasses + 1, FLT_MAX);
	//}

	//for (int j = 1; j < numClasses + 1; j++)
	//{
	//	mat1[1][j] = 1;
	//	mat2[1][j] = 0.0;
	//}
	//
	//double s1,s2,w,SSD;
	//SSD = 0.0;
	//for(int l = 2; l <= numValues; l++)	//arrays are 0 based, but we skip 1
 //   {
 //       s1=s2=w=0;
 //       for(int m = 1; m <= l; m++)
 //       {
	//		int i3 = l - m + 1;
	//		double val = (*data)[i3 - 1];
	//		s2 += val * val;
	//		s1 += val;
	//		w++;
	//		SSD = s2 - (s1 * s1) / w;
	//		int i4 = i3 - 1;

	//		if(i4 != 0 )
	//		{
	//			 for(int j = 2; j <= numClasses; j++)
	//			{
	//				double newVal = mat2[i4][j - 1] + SSD;
	//				double oldValue = mat2[l][j];
	//				if(newVal <= oldValue)		// if new class is better than previous than let's write it
	//				{
	//					mat1[l][j] = i3;
	//					mat2[l][j] = mat2[i4][j - 1] + SSD;
	//				}
	//			}
	//		}
 //       }
 //       mat1[l][0] = 0;
	//	mat2[l][0] =SSD;
	//}
	//
	std::vector<int>* result = new std::vector<int>;
	result->resize(numClasses + 1);
	(*result)[numClasses] = numValues;
 //   
	//int k = numValues;
 //   for(int j = result->size() - 1; j >= 1; j--)
 //   {
 //        int id = mat1[k][j] - 1;
 //        (*result)[j - 1] = id;
 //        k = id;
 //   }
	
	// ESRI breaks
	/*(*result)[0] = 0;
	(*result)[1] = 387;
	(*result)[2] = 1261;
	(*result)[3] = 2132;
	(*result)[4] = 2698;
	(*result)[5] = 2890;
	(*result)[6] = 2996;
	(*result)[7] = 3064;
	(*result)[8] = 3093;
	(*result)[9] = 3107;*/
	/*for (int i = 1; i <= numClasses; i++)
	{
		(*result)[i] = numValues/numClasses * i;
	}*/
	
	double step = ((*data)[numValues - 1] - (*data)[0])/(numClasses);
	int cnt = 0;
	for (int i = 0; i < numValues; i++)
	{
		if ((*data)[i] > step * cnt)
		{
			cnt++;
			if (cnt > numClasses) break;
			(*result)[cnt] = i;
		}
	}

	double s1,s2,w,SSD;
	SSD = 0;
	for (int i = 1; i< numClasses + 1; i++)
	{
		int low, high;
		if ( i == 1) low = 0;
		else low = (*result)[i-1];
		
		if ( i == numClasses) high = numValues;
		else high = (*result)[i] -1;

		s2 = s1 = w = 0;
		for (int j = low; j < high; j++)
		{
			double val = (*data)[j];
			s2 += val * val;
			s1 += val;
			w++;
		}
		if (w != 0.0)
			SSD += s2 - (s1 * s1) / w;
	}

	//// cleaning
	//for (int i = 0; i < numValues; i++)
	//{
	//	delete[] mat1[i];
	//	delete[] mat2[i];
	//}
	//delete[] mat1;
	//delete[] mat2;

	return result;
}

	// -------------------------------------------------------------------
	// Optimization routine
	// -------------------------------------------------------------------
	void Optimize()
	{
		// initialization
		double minValue = get_SumStandardDeviations();	// current best minimum
		_leftBound = 0;							// we'll consider all classes in the beginning
		_rightBound = _classes.size() - 1;
		_previousMaxId = -1;
		_previousTargetId = - 1;
		int numAttemmpts = 0;

		bool proceed = true;
		while (proceed)
		{
			for (int i = 0; i < _numValues; i++)
			{
				FindShift();
				
				// when there are only 2 classes left we should stop on the local max value
				if (_rightBound - _leftBound == 0)
				{
					double val = get_SumStandardDeviations();	// the final minimum
					numAttemmpts++; 
					
					if ( numAttemmpts > 5)
					{
						return;
					}
				}
			}
			double value = get_SumStandardDeviations();
			proceed = (value < minValue)?true:false;	// if the deviations became smaller we'll execute one more loop
			
			if (value < minValue)
				minValue = value;
		}
	}
	
	// -------------------------------------------------------------------
	// Returning of results (indices of values to start each class)
	// -------------------------------------------------------------------
	std::vector<long>* get_Results()
	{
		std::vector<long>* results = new std::vector<long>;
		results->resize(_numClasses);
		for (int i = 0; i < _numClasses; i++ )
		{
			(*results)[i] = _classes[i].startId;
		}
		return results;
	}


private:
	// data members
	std::vector<JenksData> _values;
	std::vector<JenksBreak> _classes;
	int _numClasses;
	int _numValues;
	int _previousMaxId;		// to prevent stalling in the local optimum
	int _previousTargetId;
	int _leftBound;			// the id of classes between which optimization takes place
	int _rightBound;		// initially it's all classes, then it's reducing

	// ******************************************************************
	// Calculates the sum of standard deviations of individual variants 
	// from the class mean through all class
	// It's the objective function - should be minimized
	// 
	// ******************************************************************
	double get_SumStandardDeviations()
	{
		double sum = 0.0;
		for (int i = 0; i < _numClasses; i++) 
		{
			sum += _classes[i].SDev;
		}
		return sum;
	}
	
	// ******************************************************************
	//	  MakeShift()
	// ******************************************************************
	// Passing the value from one class to another to another. Criteria - standard deviation.
	void FindShift()
	{
		// first we'll find classes with the smallest and largest SD
		int maxId = 0, minId = 0; 
		double minValue = 100000000000.0, maxValue = 0.0;	// use constant
		for (int i = _leftBound; i <= _rightBound; i++) 
		{
			if (_classes[i].SDev > maxValue)
			{
				maxValue = _classes[i].SDev;
				maxId = i;
			}

			if (_classes[i].SDev < minValue)
			{
				minValue = _classes[i].SDev;
				minId = i;
			}
		}

		// then pass one observation from the max class in the direction of min class
		int valueId = -1;
		int targetId = -1;
		if (maxId > minId)
		{
			//<-  we should find first value of max class
			valueId = _classes[maxId].startId; 
			targetId = maxId - 1;
			_classes[maxId].startId++;
			_classes[targetId].endId++;
		}
		else if (maxId < minId)
		{
			//->  we should find last value of max class
			valueId = _classes[maxId].endId; 
			targetId = maxId + 1;
			_classes[maxId].endId--;
			_classes[targetId].startId--;
		}
		else
		{
			// only one class left or the deviations withinb classes are equal
			return;
		}

		// Prevents stumbling in local optimum - algorithm will be repeating the same move
		// To prevent this we'll exclude part of classes with less standard deviation
		if (_previousMaxId == targetId && _previousTargetId == maxId)
		{
			// Now we choose which of the two states provides less deviation
			double value1 = get_SumStandardDeviations();
			
			// change to second state
			MakeShift(maxId, targetId, valueId);
			double value2 = get_SumStandardDeviations();
			
			// if first state is better revert to it
			if (value1 < value2)
			{
				MakeShift(targetId, maxId, valueId);
			}
			
			// now we can exclude part of the classes where no improvements can be expected
			int min = MIN(targetId, maxId);
			int max = MAX(targetId, maxId);
			
			double avg = get_SumStandardDeviations()/(_rightBound - _leftBound + 1);

			// analyze left side of distribution
			double sumLeft = 0, sumRight = 0;
			for (int j = _leftBound; j <= min; j++)
			{
				sumLeft += pow(_classes[j].SDev - avg, 2.0);
			}
			sumLeft /= (min - _leftBound + 1);

			// analyze right side of distribution
			for (int j = _rightBound; j >= max; j--)
			{
				sumRight += pow(_classes[j].SDev - avg, 2.0);
			}
			sumRight /= (_rightBound - max + 1);

			// exluding left part
			if (sumLeft >= sumRight)
			{
				_leftBound = max;
			}
			// exluding right part
			else if (sumLeft < sumRight)
			{
				_rightBound = min;
			}
		}
		else
		{
			MakeShift(maxId, targetId, valueId);
		}
	}

	// perform actual shift
	void MakeShift(int maxId, int targetId, int valueId)
	{
		// saving the last shift
		_previousMaxId = maxId;
		_previousTargetId = targetId;

		JenksData* data = &(_values[valueId]);

		// removing from max class
		_classes[maxId].value -= data->value;
		_classes[maxId].square -= data->square;
		_classes[maxId].count -= 1;
		_classes[maxId].RefreshStandardDeviations();
		
		// passing to target class
		_classes[targetId].value += data->value;
		_classes[targetId].square += data->square;
		_classes[targetId].count += 1;
		_classes[targetId].RefreshStandardDeviations();
		
		// mark that the value was passed
		_values[valueId].classId = targetId;
	}
};
