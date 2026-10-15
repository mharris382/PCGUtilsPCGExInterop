// Copyright Max Harris

#include "Customizations/PCGUtilsCollectionCategoryHandleCustomization.h"

#include "Collections/PCGUtilsCollectionCategoryHandle.h"
#include "Core/PCGExAssetCollection.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "Styling/AppStyle.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PCGUtilsCollectionCategoryHandleCustomization"

TSharedRef<IPropertyTypeCustomization> FPCGUtilsCollectionCategoryHandleCustomization::MakeInstance()
{
	return MakeShared<FPCGUtilsCollectionCategoryHandleCustomization>();
}

void FPCGUtilsCollectionCategoryHandleCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructHandle = PropertyHandle;
	CollectionHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGUtilsCollectionCategoryHandle, Collection));
	CategoryHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGUtilsCollectionCategoryHandle, Category));

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250.f)
		[
			SNew(STextBlock)
			.Text(this, &FPCGUtilsCollectionCategoryHandleCustomization::GetHeaderText)
			.ColorAndOpacity(this, &FPCGUtilsCollectionCategoryHandleCustomization::GetCategoryColor)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];
}

void FPCGUtilsCollectionCategoryHandleCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	if (!CollectionHandle.IsValid() || !CategoryHandle.IsValid())
	{
		return;
	}

	CollectionHandle->SetOnPropertyValueChanged(
		FSimpleDelegate::CreateSP(this, &FPCGUtilsCollectionCategoryHandleCustomization::RefreshCategories));
	RefreshCategories();

	ChildBuilder.AddProperty(CollectionHandle.ToSharedRef());

	ChildBuilder.AddCustomRow(CategoryHandle->GetPropertyDisplayName())
		.PropertyHandleList({CategoryHandle})
		.NameContent()
		[
			CategoryHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250.f)
		[
			SAssignNew(CategoryComboBox, SComboBox<TSharedPtr<FName>>)
			.OptionsSource(&CategoryOptions)
			.IsEnabled(this, &FPCGUtilsCollectionCategoryHandleCustomization::CanPickCategory)
			// Entries can be recategorized in the collection editor while this panel stays open.
			.OnComboBoxOpening(this, &FPCGUtilsCollectionCategoryHandleCustomization::RefreshCategories)
			.OnGenerateWidget(this, &FPCGUtilsCollectionCategoryHandleCustomization::MakeCategoryOptionWidget)
			.OnSelectionChanged(this, &FPCGUtilsCollectionCategoryHandleCustomization::OnCategorySelected)
			.ToolTipText(this, &FPCGUtilsCollectionCategoryHandleCustomization::GetCategoryToolTip)
			[
				SNew(STextBlock)
				.Text(this, &FPCGUtilsCollectionCategoryHandleCustomization::GetCategoryText)
				.ColorAndOpacity(this, &FPCGUtilsCollectionCategoryHandleCustomization::GetCategoryColor)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];
}

UPCGExAssetCollection* FPCGUtilsCollectionCategoryHandleCustomization::GetSharedCollection(bool& bOutMultipleCollections) const
{
	bOutMultipleCollections = false;
	if (!StructHandle.IsValid())
	{
		return nullptr;
	}

	TArray<void*> RawData;
	StructHandle->AccessRawData(RawData);

	const TSoftObjectPtr<UPCGExAssetCollection>* Shared = nullptr;
	for (const void* Raw : RawData)
	{
		if (!Raw)
		{
			continue;
		}
		const TSoftObjectPtr<UPCGExAssetCollection>& Collection =
			static_cast<const FPCGUtilsCollectionCategoryHandle*>(Raw)->Collection;
		if (Shared && *Shared != Collection)
		{
			bOutMultipleCollections = true;
			return nullptr;
		}
		Shared = &Collection;
	}

	// Get, not LoadSynchronous: this runs every frame from the text bindings. Picking a collection in the
	// asset picker loads it, and RefreshCategories loads it when the panel opens.
	return Shared ? Shared->Get() : nullptr;
}

void FPCGUtilsCollectionCategoryHandleCustomization::RefreshCategories()
{
	if (StructHandle.IsValid())
	{
		TArray<void*> RawData;
		StructHandle->AccessRawData(RawData);
		for (const void* Raw : RawData)
		{
			if (Raw)
			{
				static_cast<const FPCGUtilsCollectionCategoryHandle*>(Raw)->LoadCollection();
			}
		}
	}

	bool bMultipleCollections = false;
	const TArray<FName> Names =
		FPCGUtilsCollectionCategoryHandle::GetCategoryNames(GetSharedCollection(bMultipleCollections));

	KnownCategories = TSet<FName>(Names);
	CategoryOptions.Reset();
	// None first, so a category can be cleared from the dropdown.
	CategoryOptions.Add(MakeShared<FName>(NAME_None));
	for (const FName Name : Names)
	{
		CategoryOptions.Add(MakeShared<FName>(Name));
	}

	if (CategoryComboBox.IsValid())
	{
		CategoryComboBox->RefreshOptions();
	}
}

void FPCGUtilsCollectionCategoryHandleCustomization::OnCategorySelected(TSharedPtr<FName> Selected, ESelectInfo::Type SelectInfo)
{
	if (Selected.IsValid() && CategoryHandle.IsValid() && SelectInfo != ESelectInfo::Direct)
	{
		CategoryHandle->SetValue(*Selected);
	}
}

TSharedRef<SWidget> FPCGUtilsCollectionCategoryHandleCustomization::MakeCategoryOptionWidget(TSharedPtr<FName> Option) const
{
	return SNew(STextBlock)
		.Text(Option.IsValid() ? FText::FromName(*Option) : FText::GetEmpty())
		.Font(IDetailLayoutBuilder::GetDetailFont());
}

FText FPCGUtilsCollectionCategoryHandleCustomization::GetCategoryText() const
{
	FName Category;
	if (!CategoryHandle.IsValid())
	{
		return FText::GetEmpty();
	}
	if (CategoryHandle->GetValue(Category) == FPropertyAccess::MultipleValues)
	{
		return LOCTEXT("MultipleValues", "Multiple Values");
	}
	return FText::FromName(Category);
}

FText FPCGUtilsCollectionCategoryHandleCustomization::GetCategoryToolTip() const
{
	bool bMultipleCollections = false;
	const UPCGExAssetCollection* Collection = GetSharedCollection(bMultipleCollections);
	if (bMultipleCollections)
	{
		return LOCTEXT("MultipleCollections",
			"The selected objects reference different collections; give them one collection to choose a category.");
	}
	if (!Collection)
	{
		return LOCTEXT("NoCollection", "Assign a collection to choose one of its categories.");
	}
	if (IsCategoryMissing())
	{
		FName Category;
		CategoryHandle->GetValue(Category);
		return FText::Format(
			LOCTEXT("MissingCategory", "No entry in {0} uses the category '{1}'."),
			FText::FromString(Collection->GetName()), FText::FromName(Category));
	}
	return FText::Format(
		LOCTEXT("PickCategory", "A category used by the entries of {0}."), FText::FromString(Collection->GetName()));
}

FSlateColor FPCGUtilsCollectionCategoryHandleCustomization::GetCategoryColor() const
{
	return IsCategoryMissing() ? FAppStyle::GetSlateColor("Colors.Error") : FSlateColor::UseForeground();
}

FText FPCGUtilsCollectionCategoryHandleCustomization::GetHeaderText() const
{
	bool bMultipleCollections = false;
	const UPCGExAssetCollection* Collection = GetSharedCollection(bMultipleCollections);
	if (bMultipleCollections)
	{
		return LOCTEXT("HeaderMultiple", "Multiple Values");
	}
	if (!Collection)
	{
		return LOCTEXT("HeaderNone", "None");
	}
	return FText::Format(LOCTEXT("Header", "{0} : {1}"), FText::FromString(Collection->GetName()), GetCategoryText());
}

bool FPCGUtilsCollectionCategoryHandleCustomization::CanPickCategory() const
{
	bool bMultipleCollections = false;
	return GetSharedCollection(bMultipleCollections) != nullptr;
}

bool FPCGUtilsCollectionCategoryHandleCustomization::IsCategoryMissing() const
{
	FName Category;
	if (!CategoryHandle.IsValid() || CategoryHandle->GetValue(Category) != FPropertyAccess::Success || Category.IsNone())
	{
		return false;
	}
	bool bMultipleCollections = false;
	return GetSharedCollection(bMultipleCollections) && !KnownCategories.Contains(Category);
}

#undef LOCTEXT_NAMESPACE
