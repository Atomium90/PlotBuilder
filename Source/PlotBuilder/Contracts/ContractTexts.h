#pragma once

#include "CoreMinimal.h"
#include "Contracts/ContractTypes.h"

/**
 *  The words the player reads about a contract: the type, the site, the time, the one-line job, and each condition in a short form
 *  (the chip on the card) and a long one (the details). The board, the notebook and the debrief all read these, so a contract is
 *  never worded two ways. Plain functions on plain data, so they are tested without a level or a widget.
 */
namespace ContractTexts
{
	/** One condition in words: Chip for the card ("3 pieces", "No jackhammer"), Title and Detail for the details strip and the notebook. */
	struct FConditionText
	{
		EConditionType Type = EConditionType::SalvageFurniture;
		FText Chip;
		FText Title;
		FText Detail;
	};

	/** "Total demolition", "Partial demolition", "Renovation". */
	FText TypeLabel(EContractType Type);

	/** What is on the site, from the archetype the contract asks for: "Bungalow", "Two-storey house", "Three-storey house", "Apartment block". An unknown archetype reads "House". */
	FText SiteName(const FContractDefinition& Contract);

	/** The first letters of the first two words of a name, for the square that stands in for a portrait: "Brandt & Sons" gives "BS". */
	FString Initials(const FString& Name);

	/** The estimated minutes rounded to the half minute, without a unit: "5", "10.5". */
	FString MinutesString(float Minutes);

	/** What the job is, in one sentence: "Take the whole bungalow down." */
	FText Job(const FContractDefinition& Contract);

	/** The contract on one line for the notebook: the site and its size, then the job. */
	FText Summary(const FContractDefinition& Contract);

	/** The conditions of the contract in the order it lists them. */
	TArray<FConditionText> Conditions(const FContractDefinition& Contract);
}
