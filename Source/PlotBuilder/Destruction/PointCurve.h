#pragma once

#include "CoreMinimal.h"

/**
 *  A curve given as a list of points (x, y), sorted by x: a straight line between two neighbours, flat beyond the first
 *  and the last. The data curves of the project (what a damaged piece still transmits to what rests on it, how much of a
 *  damaged piece of furniture is still worth) are lists of points in a Data Asset, not curve assets: one mechanism to
 *  explain, nothing to create or reassign, and the interpolation runs without the engine, so it is unit-tested.
 */
namespace PointCurve
{
	/** y at X. Points must be sorted by x and hold at least one point (with one, its y whatever X is). */
	inline float Evaluate(const TArray<FVector2D>& Points, float X)
	{
		check(!Points.IsEmpty());

		if (X <= Points[0].X)
		{
			return static_cast<float>(Points[0].Y);
		}

		for (int32 Index = 1; Index < Points.Num(); ++Index)
		{
			if (X <= Points[Index].X)
			{
				const FVector2D& From = Points[Index - 1];
				const FVector2D& To = Points[Index];
				const double Span = To.X - From.X;

				// Two points at the same x: a step, the upper one wins.
				return Span > KINDA_SMALL_NUMBER ? static_cast<float>(FMath::Lerp(From.Y, To.Y, (X - From.X) / Span)) : static_cast<float>(To.Y);
			}
		}

		return static_cast<float>(Points.Last().Y);
	}
}
