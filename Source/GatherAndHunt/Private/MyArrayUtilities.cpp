// Fill out your copyright notice in the Description page of Project Settings.


#include "MyArrayUtilities.h"
#include <algorithm>

void UMyArrayUtilities::SortIntArray(TArray<int32>& ArrayToSort, bool bAscending)
{
	if (bAscending)
	{
		ArrayToSort.Sort();
	}
	else
	{
		ArrayToSort.Sort([](const int32 A, const int32 B) {
			return A > B; // For descending order
		});
	}
}


TArray<int32> UMyArrayUtilities::SplitAndRearrangeArray(TArray<int32> InputArray, int32 SplitValue)
{
    TArray<int32> BeforeSplit;
    TArray<int32> AfterSplit;
    bool bFoundSplitValue = false;

    // Loop through the array to find the split value
    for (int32 Value : InputArray)
    {
        if (Value == SplitValue)
        {
            bFoundSplitValue = true;
            continue; // Skip adding the split value itself
        }

        if (!bFoundSplitValue)
        {
            BeforeSplit.Add(Value);
        }
        else
        {
            AfterSplit.Add(Value);
        }
    }

    // Combine the arrays: everything after the split value, followed by everything before it
    AfterSplit.Append(BeforeSplit);

    return AfterSplit;
}

UMyArrayUtilities::UMyArrayUtilities()
{
}

UMyArrayUtilities::~UMyArrayUtilities()
{
}
