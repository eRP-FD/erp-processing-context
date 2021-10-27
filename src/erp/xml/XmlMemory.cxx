/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "XmlMemory.hxx"

template class std::unique_ptr<FreeXml<xmlFreeDoc>::Type, FreeXml<xmlFreeDoc>>;
template class std::unique_ptr<FreeXml<xmlFreeNode>::Type, FreeXml<xmlFreeNode>>;
template class std::unique_ptr<FreeXml<xmlFreeNs>::Type, FreeXml<xmlFreeNs>>;
