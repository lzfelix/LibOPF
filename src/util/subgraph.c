/*
  Copyright (C) <2009> <Alexandre Xavier Falcão and João Paulo Papa>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  please see full copyright in COPYING file.
  -------------------------------------------------------------------------
  written by A.X. Falcão <afalcao@ic.unicamp.br> and by J.P. Papa
  <papa.joaopaulo@gmail.com>, Oct 20th 2008

  This program is a collection of functions to manage the Optimum-Path Forest (OPF)
  classifier.*/

#include "subgraph.h"

/*----------- Constructor and destructor ------------------------*/
// Allocate nodes without features
Subgraph *CreateSubgraph(int nnodes)
{
  Subgraph *sg = (Subgraph *)calloc(1, sizeof(Subgraph));
  int i;

  sg->nnodes = nnodes;
  sg->node = (SNode *)calloc(nnodes, sizeof(SNode));
  sg->ordered_list_of_nodes = (int *)calloc(nnodes, sizeof(int));

  if (sg->node == NULL)
  {
    Error("Cannot allocate nodes", "CreateSubgraph");
  }

  sg->bestk = 0;

  for (i = 0; i < sg->nnodes; i++)
  {
    sg->node[i].feat = NULL;
    sg->node[i].relevant = 0;
    sg->node[i].dens = 0.0;
  }

  return (sg);
}

// Deallocate memory for subgraph
void DestroySubgraph(Subgraph **sg)
{
  int i;

  if ((*sg) != NULL)
  {
    for (i = 0; i < (*sg)->nnodes; i++)
    {
      if ((*sg)->node[i].feat != NULL)
        free((*sg)->node[i].feat);
      if ((*sg)->node[i].adj != NULL)
        DestroySet(&(*sg)->node[i].adj);
    }
    free((*sg)->node);
    free((*sg)->ordered_list_of_nodes);
    free((*sg));
    *sg = NULL;
  }
}

//write subgraph to disk
void WriteSubgraph(Subgraph *g, char *file)
{
  FILE *fp = NULL;
  int i, j;

  if (g != NULL)
  {
    fp = fopen(file, "wb");
    fwrite(&g->nnodes, sizeof(int), 1, fp);
    fwrite(&g->nlabels, sizeof(int), 1, fp);
    fwrite(&g->nfeats, sizeof(int), 1, fp);
    fprintf(stderr, "Samples: %d\nLabels: %d\nFeatures: %d", g->nnodes, g->nlabels, g->nfeats);

    /*writing position(id), label and features*/
    for (i = 0; i < g->nnodes; i++)
    {
      fwrite(&g->node[i].position, sizeof(int), 1, fp);
      fwrite(&g->node[i].truelabel, sizeof(int), 1, fp);
      for (j = 0; j < g->nfeats; j++)
        fwrite(&g->node[i].feat[j], sizeof(float), 1, fp);
    }
    fclose(fp);
  }
}

//creates a new sub-graph from a graph's prototypes. This method doesn't copy the node's root index.
Subgraph *CreateGraphFromPrototypes(Subgraph* graph) {

  // creating the graph
  int amount_prototypes = 0;
  for (int i = 0; i < graph->nnodes; i++) {
    if (graph->node[i].pred == NIL) {
      amount_prototypes++;
    }
  }
  Subgraph* new_graph = CreateSubgraph(amount_prototypes);
  new_graph->nlabels = 1;
  new_graph->nfeats = graph->nfeats;

  // adding the prototypes
  int node_index = 0;
  for (int i = 0; i < graph->nnodes; i++) {
    if (graph->node[i].pred == NIL) {

      // cloning node from one graph to the other.
      new_graph->node[node_index].feat = AllocFloatArray(graph->nfeats);
      for (int j = 0; j < graph->nfeats; j++) {
        new_graph->node[node_index].feat[j] = graph->node[i].feat[j];
      }

      // copying the prototype true label (it really doesn't matter for now) and its position,
      // so the distance matrix can be reuse across depth levels.
      new_graph->node[node_index].truelabel = graph->node[i].truelabel;
      new_graph->node[node_index].label = graph->node[i].label;
      new_graph->node[node_index].pred = graph->node[i].pred;
      new_graph->node[node_index].position = graph->node[i].position;

      node_index++;
    }
  }

  return new_graph;
}

//read subgraph from opf format file
Subgraph *ReadSubgraph(char *file)
{
  Subgraph *g = NULL;
  FILE *fp = NULL;
  int nnodes, i, j;
  char msg[256];

  if ((fp = fopen(file, "rb")) == NULL)
  {
    sprintf(msg, "%s%s", "Unable to open file ", file);
    Error(msg, "ReadSubGraph");
  }

  /*reading # of nodes, classes and feats*/
  if (fread(&nnodes, sizeof(int), 1, fp) != 1)
    Error("Could not read the number of nodes", "ReadSubGraph");
  g = CreateSubgraph(nnodes);
  if (fread(&g->nlabels, sizeof(int), 1, fp) != 1)
    Error("Could not read the number of labels", "ReadSubGraph");
  if (fread(&g->nfeats, sizeof(int), 1, fp) != 1)
    Error("Could not read the number of features", "ReadSubGraph");

  /*reading features*/
  for (i = 0; i < g->nnodes; i++)
  {
    g->node[i].feat = AllocFloatArray(g->nfeats);
    if (fread(&g->node[i].position, sizeof(int), 1, fp) != 1)
      Error("Could not read node position", "ReadSubGraph");
    if (fread(&g->node[i].truelabel, sizeof(int), 1, fp) != 1)
      Error("Could not read node true label", "ReadSubGraph");

    for (j = 0; j < g->nfeats; j++)
      if (fread(&g->node[i].feat[j], sizeof(float), 1, fp) != 1)
        Error("Could not read node features", "ReadSubGraph");
  }

  fclose(fp);

  return g;
}

// Copy subgraph (does not copy Arcs)
Subgraph *CopySubgraph(Subgraph *g)
{
  Subgraph *clone = NULL;
  int i;

  if (g != NULL)
  {
    clone = CreateSubgraph(g->nnodes);

    clone->bestk = g->bestk;
    clone->df = g->df;
    clone->nlabels = g->nlabels;
    clone->nfeats = g->nfeats;
    clone->mindens = g->mindens;
    clone->maxdens = g->maxdens;
    clone->K = g->K;

    for (i = 0; i < g->nnodes; i++)
    {
      CopySNode(&clone->node[i], &g->node[i], g->nfeats);
      clone->ordered_list_of_nodes[i] = g->ordered_list_of_nodes[i];
    }

    return clone;
  }
  else
    return NULL;
}

//Copy nodes
void CopySNode(SNode *dest, SNode *src, int nfeats)
{
  dest->feat = AllocFloatArray(nfeats);
  memcpy(dest->feat, src->feat, nfeats * sizeof(float));
  dest->pathval = src->pathval;
  dest->dens = src->dens;
  dest->label = src->label;
  dest->root = src->root;
  dest->pred = src->pred;
  dest->truelabel = src->truelabel;
  dest->position = src->position;
  dest->status = src->status;
  dest->relevant = src->relevant;
  dest->radius = src->radius;
  dest->nplatadj = src->nplatadj;

  dest->adj = CloneSet(src->adj);
}

//Swap nodes
void SwapSNode(SNode *a, SNode *b)
{
  SNode tmp;

  tmp = *a;
  *a = *b;
  *b = tmp;
}
