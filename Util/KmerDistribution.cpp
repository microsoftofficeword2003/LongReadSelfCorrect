//-----------------------------------------------
// Copyright 2010 Wellcome Trust Sanger Institute
// Written by Jared Simpson (js18@sanger.ac.uk)
// Released under the GPL license
//-----------------------------------------------
//
// KmerDistribution - Histogram of kmer frequencies
//
#include "KmerDistribution.h"
#include <cstdlib>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <limits>
#include <algorithm>
#include <vector>       // std::vector
#include <math.h>

double KmerDistribution::getCumulativeProportionLEQ(int n) const
{    
	size_t cumulativeSum = 0;
	double cumulativeProportion = 0;
	for(iteratorKmerFreqsMap iterElement = m_data.begin(); iterElement != m_data.end(); iterElement++)
	{
		if(iterElement->first > n)
			break;
		cumulativeSum +=  iterElement->second;
		cumulativeProportion = (double)cumulativeSum/m_total;
	}
	return cumulativeProportion;
}

size_t KmerDistribution::getCutoffForProportion(double p) const
{
	if(p > 1 || p < 0)
	{
		std::cout<<"Portion should between 0 <-> 1.\n";
		exit(EXIT_FAILURE);
	}
	size_t kmerFreqs = 0;
	size_t cumulativeSum = 0;
	double cumulativeProportion = 0;
	for(iteratorKmerFreqsMap iterElement = m_data.begin(); iterElement != m_data.end(); iterElement++)
	{
		kmerFreqs = iterElement->first;
		cumulativeSum += iterElement->second;
		cumulativeProportion = (double)cumulativeSum/m_total;
		if(cumulativeProportion > p)
			break;
	}
	return kmerFreqs;
}
//compute median and std
void KmerDistribution::computeKDAttributes()
{
	std::vector<size_t>rawdata;
	for(iteratorKmerFreqsMap iterElement = m_data.begin(); iterElement !=  m_data.end(); iterElement++)
		rawdata.resize((rawdata.size() + iterElement->second), iterElement->first);
    
	//compute quartiles
	m_median = rawdata[rawdata.size()/2];
	m_first_quartile = rawdata[rawdata.size()/4];
	m_third_quartile = rawdata[rawdata.size()*3/4];
	
	//compute standard deviation
	double difference_square = 0;
	for(std::vector<size_t>::iterator iterFreq = rawdata.begin(); iterFreq != rawdata.end(); iterFreq++)
		difference_square += pow(((*iterFreq) - m_median), 2);
	double variance = difference_square / (rawdata.size() - 1);
	m_std = sqrt(variance);
	
	// double freq95 = getCutoffForProportion(0.95);
	// m_repeatKmerCutoff = m_std > m_median*2? m_median*1.5: m_median*1.3;
	m_repeatKmerCutoff = m_median*1.3;
	// m_repeatKmerCutoff = getCutoffForProportion(0.8);
	// m_repeatKmerCutoff = (double) m_median*(0.39+0.53* (freq95/(double)m_median));
	
}
void KmerDistribution::write(std::ofstream& outfile)
{
	for(iteratorKmerFreqsMap iterElement = m_data.begin(); iterElement !=  m_data.end(); iterElement++)
		outfile << iterElement->first << "\t" << iterElement->second << "\n";
}
size_t KmerDistribution::getCensoredMode(size_t n) const
{
	size_t most = 0,mode = 0;
    iteratorKmerFreqsMap iterElement = m_data.begin();
	while(iterElement->first < n)
		iterElement++;
	for(; iterElement != m_data.end(); iterElement++)
		if(iterElement->second > most)
		{
			mode = iterElement->first;
			most = iterElement->second;
		}
	return mode;
}

//Below are legacy codes. Noted by KuanWeiLee 20171027
/***********************************************************************************/
int KmerDistribution::findFirstLocalMinimum() const
{
    std::vector<int> countVector = toCountVector(1000);
    if(countVector.empty())
        return -1;

    std::cout << "CV: " << countVector.size() << "\n";
    int prevCount = countVector[1];
    double threshold = 0.75;
    for(size_t i = 2; i < countVector.size(); ++i)
    {
        int currCount = countVector[i];
        double ratio = (double)currCount / prevCount;
        std::cout << i << " " << currCount << " " << ratio << "\n";
        if(ratio > threshold)
            return i;
        prevCount = currCount;
    }
    return -1;
}


// Find the boundary of the kmers that are likely erroneous
// We do this by finding the value between 1 and the trusted mode
// that contributes the fewest
int KmerDistribution::findErrorBoundary() const
{
    int mode = getCensoredMode(5);
    if(mode == -1)
        return -1;

    std::cerr << "Trusted kmer mode: " << mode  << "\n";
    std::vector<int> countVector = toCountVector(1000);
    if(countVector.empty())
        return -1;

    int runningSum = 0;
    double minContrib = std::numeric_limits<double>::max();
    int idx = -1;
    for(int i = 1; i < mode; ++i)
    {
        runningSum += countVector[i];
        double v = (double)countVector[i] / runningSum;
        if(v < minContrib)
        {
            minContrib = v;
            idx = i;
        }
    }
    return idx;
}

//Similar to findFirstLocalMinimum() with a few different parameters 
int KmerDistribution::findErrorBoundaryByRatio(double ratio) const
{
    int mode = getCensoredMode(5);
    if(mode == -1)
        return -1;

    std::cerr << "Trusted kmer mode: " << mode  << "\n";
    std::vector<int> countVector = toCountVector(1000);
    if(countVector.empty())
        return -1;

    for(int i = 1; i < mode - 1; ++i)
    {
        int currCount = countVector[i];
        int nextCount  = countVector[i+1];
        double cr = (double)currCount / nextCount;
        if(cr < ratio)
            return i;
    }
    return -1;
}

// 
std::vector<int> KmerDistribution::toCountVector(int max) const
{
    std::vector<int> out;
    if(m_data.empty())
        return out;

    int min = 0;

    for(int i = min; i <= max; ++i)
    {
        std::map<size_t,size_t>::const_iterator iter = m_data.find(i);
        int v = (iter != m_data.end()) ? iter->second : 0;
        out.push_back(v);
    }
    return out;
}

// for compatibility with old code
void KmerDistribution::print(int max) const
{
    print(stdout, max);
}

void KmerDistribution::print(FILE* fp, int max) const
{
    fprintf(fp, "Kmer coverage histogram\n");
    fprintf(fp, "cov\tcount\n");

    int maxCount = 0;
    std::map<size_t,size_t>::const_iterator iter = m_data.begin();
    for(; iter != m_data.end(); ++iter)
    {
        if(iter->first <= max)
            fprintf(fp, "%d\t%d\n", iter->first, iter->second);
        else
            maxCount += iter->second;
    }
    fprintf(fp, ">%d\t%d\n", max, maxCount);

}

