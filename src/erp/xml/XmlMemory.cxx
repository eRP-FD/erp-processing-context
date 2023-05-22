/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "XmlMemory.hxx"

template class std::unique_ptr<FreeXml<xmlFreeDoc>::Type, FreeXml<xmlFreeDoc>>;
template class std::unique_ptr<FreeXml<xmlFreeNode>::Type, FreeXml<xmlFreeNode>>;
template class std::unique_ptr<FreeXml<xmlFreeNs>::Type, FreeXml<xmlFreeNs>>;
