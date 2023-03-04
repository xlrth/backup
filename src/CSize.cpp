#include "CSize.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CSize::CSize(long long value)
    :
    mValue(value)
{}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
CSize::operator long long() const
{
    return mValue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSize::IsSpecified() const
{
    return mValue != UNSPECIFIED;
}
